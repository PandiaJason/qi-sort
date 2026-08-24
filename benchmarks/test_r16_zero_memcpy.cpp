#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <cstdint>

using u32 = uint32_t;

class ScratchBuffer {
public:
    u32* get(size_t n) {
        if (n > capacity_) {
            buf_.resize(n);
            capacity_ = n;
        }
        return buf_.data();
    }
private:
    std::vector<u32> buf_;
    size_t capacity_ = 0;
};

inline ScratchBuffer& getScratch() {
    static thread_local ScratchBuffer scratch;
    return scratch;
}

// 2-Pass Radix-16 Zero-Memcpy Engine (Pass 0: data->buf, Pass 1: buf->data)
// Cuts memory traffic from 64 MB down to 32 MB!
inline void radixSort16_zero_memcpy(u32* data, size_t n) {
    if (n <= 1) return;
    u32* buf = getScratch().get(n);

    alignas(64) static thread_local uint32_t c0[65536];
    alignas(64) static thread_local uint32_t c1[65536];
    std::memset(c0, 0, sizeof(c0));
    std::memset(c1, 0, sizeof(c1));

    // 1 combined count pass
    for (size_t i = 0; i < n; ++i) {
        u32 v = data[i];
        c0[v & 0xFFFFu]++;
        c1[v >> 16]++;
    }

    uint32_t s0 = 0, s1 = 0;
    for (int i = 0; i < 65536; ++i) {
        uint32_t t0 = c0[i]; c0[i] = s0; s0 += t0;
        uint32_t t1 = c1[i]; c1[i] = s1; s1 += t1;
    }

    // Pass 0: data -> buf (bits 0-15)
    for (size_t i = 0; i < n; ++i) {
        u32 v = data[i];
        buf[c0[v & 0xFFFFu]++] = v;
    }

    // Pass 1: buf -> data (bits 16-31) — ENDS DIRECTLY IN data! NO MEMCPY!
    for (size_t i = 0; i < n; ++i) {
        u32 v = buf[i];
        data[c1[v >> 16]++] = v;
    }
}

int main() {
    constexpr size_t N = 2'000'000;
    std::vector<uint32_t> data(N);
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFFu);

    for (size_t i = 0; i < N; ++i) data[i] = dist(rng);

    auto test_data = data;
    radixSort16_zero_memcpy(test_data.data(), N);
    bool ok = std::is_sorted(test_data.begin(), test_data.end());
    std::cout << "[radixSort16_zero_memcpy] Correctness: " << (ok ? "PASS" : "FAIL") << "\n";

    double min_r16 = 1e9, min_std = 1e9;
    for (int run = 0; run < 5; ++run) {
        auto d1 = data;
        auto t0 = std::chrono::high_resolution_clock::now();
        radixSort16_zero_memcpy(d1.data(), N);
        auto t1 = std::chrono::high_resolution_clock::now();
        double dt1 = std::chrono::duration<double, std::milli>(t1 - t0).count();
        min_r16 = std::min(min_r16, dt1);

        auto d2 = data;
        t0 = std::chrono::high_resolution_clock::now();
        std::sort(d2.begin(), d2.end());
        t1 = std::chrono::high_resolution_clock::now();
        double dt2 = std::chrono::duration<double, std::milli>(t1 - t0).count();
        min_std = std::min(min_std, dt2);
    }

    std::cout << "std::sort:                 " << min_std << " ms\n";
    std::cout << "radixSort16_zero_memcpy:   " << min_r16 << " ms  (" << (min_std / min_r16) << "x speedup)\n";

    return 0;
}
