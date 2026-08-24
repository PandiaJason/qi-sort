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

inline void radixSort11_fast(u32* data, size_t n, u32 bitOr = ~0u) {
    if (n <= 1) return;
    u32* buf = getScratch().get(n);

    alignas(64) uint32_t c0[2048] = {};
    alignas(64) uint32_t c1[2048] = {};
    alignas(64) uint32_t c2[1024] = {};

    // 1 combined count pass (20 KB histograms fit in 32 KB L1)
    for (size_t i = 0; i < n; ++i) {
        u32 v = data[i];
        c0[v & 0x7FFu]++;
        c1[(v >> 11) & 0x7FFu]++;
        c2[v >> 22]++;
    }

    uint32_t s0 = 0, s1 = 0, s2 = 0;
    for (int i = 0; i < 2048; ++i) {
        uint32_t t;
        t = c0[i]; c0[i] = s0; s0 += t;
        t = c1[i]; c1[i] = s1; s1 += t;
        if (i < 1024) { t = c2[i]; c2[i] = s2; s2 += t; }
    }

    // Pass 0: data -> buf
    for (size_t i = 0; i < n; ++i) {
        u32 v = data[i];
        buf[c0[v & 0x7FFu]++] = v;
    }

    // Pass 1: buf -> data
    for (size_t i = 0; i < n; ++i) {
        u32 v = buf[i];
        data[c1[(v >> 11) & 0x7FFu]++] = v;
    }

    // Pass 2 (optional): data -> buf
    if ((bitOr >> 22) != 0) {
        for (size_t i = 0; i < n; ++i) {
            u32 v = data[i];
            buf[c2[v >> 22]++] = v;
        }
        std::memcpy(data, buf, n * sizeof(u32));
    }
}

int main() {
    constexpr size_t N = 2'000'000;
    std::vector<uint32_t> data(N);
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFFu);

    for (size_t i = 0; i < N; ++i) data[i] = dist(rng);

    auto test_data = data;
    radixSort11_fast(test_data.data(), N);
    bool ok = std::is_sorted(test_data.begin(), test_data.end());
    std::cout << "[radixSort11_fast] Correctness: " << (ok ? "PASS" : "FAIL") << "\n";

    double min_fast = 1e9, min_std = 1e9;
    for (int run = 0; run < 5; ++run) {
        auto d1 = data;
        auto t0 = std::chrono::high_resolution_clock::now();
        radixSort11_fast(d1.data(), N);
        auto t1 = std::chrono::high_resolution_clock::now();
        double dt1 = std::chrono::duration<double, std::milli>(t1 - t0).count();
        min_fast = std::min(min_fast, dt1);

        auto d2 = data;
        t0 = std::chrono::high_resolution_clock::now();
        std::sort(d2.begin(), d2.end());
        t1 = std::chrono::high_resolution_clock::now();
        double dt2 = std::chrono::duration<double, std::milli>(t1 - t0).count();
        min_std = std::min(min_std, dt2);
    }

    std::cout << "std::sort:           " << min_std << " ms\n";
    std::cout << "radixSort11_fast:   " << min_fast << " ms  (" << (min_std / min_fast) << "x speedup)\n";

    return 0;
}
