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

inline void radixSort16_fast(u32* data, size_t n) {
    if (n <= 1) return;
    u32* buf = getScratch().get(n);

    alignas(64) uint32_t c0[65536] = {};
    alignas(64) uint32_t c1[65536] = {};

    // 1 combined count pass
    for (size_t i = 0; i < n; ++i) {
        u32 v = data[i];
        c0[v & 0xFFFFu]++;
        c1[v >> 16]++;
    }

    bool skipPass1 = (c1[0] == n);

    uint32_t s0 = 0, s1 = 0;
    for (int i = 0; i < 65536; ++i) {
        uint32_t t0 = c0[i]; c0[i] = s0; s0 += t0;
        if (!skipPass1) { uint32_t t1 = c1[i]; c1[i] = s1; s1 += t1; }
    }

    // Pass 0: data -> buf (bits 0-15)
    for (size_t i = 0; i < n; ++i) {
        u32 v = data[i];
        buf[c0[v & 0xFFFFu]++] = v;
    }

    if (skipPass1) {
        std::memcpy(data, buf, n * sizeof(u32));
        return;
    }

    // Pass 1: buf -> data (bits 16-31) — ENDS IN data! 0 MEMCPY!
    for (size_t i = 0; i < n; ++i) {
        u32 v = buf[i];
        data[c1[v >> 16]++] = v;
    }
}

int main() {
    constexpr size_t N = 2'000'000;
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFFu);

    std::vector<uint32_t> uniform(N), dupes(N), nearly(N);
    for (size_t i = 0; i < N; ++i) {
        uniform[i] = dist(rng);
        dupes[i] = rng() % 256;
    }
    nearly = uniform;
    std::sort(nearly.begin(), nearly.end());
    for (size_t i = 0; i < N / 20; ++i) nearly[rng() % N] = dist(rng);

    auto run_bench = [](const std::string& name, std::vector<uint32_t>& data) {
        auto test = data;
        radixSort16_fast(test.data(), test.size());
        bool ok = std::is_sorted(test.begin(), test.end());

        double min_r16 = 1e9, min_std = 1e9;
        for (int r = 0; r < 5; ++r) {
            auto d1 = data;
            auto t0 = std::chrono::high_resolution_clock::now();
            radixSort16_fast(d1.data(), d1.size());
            auto t1 = std::chrono::high_resolution_clock::now();
            min_r16 = std::min(min_r16, std::chrono::duration<double, std::milli>(t1 - t0).count());

            auto d2 = data;
            t0 = std::chrono::high_resolution_clock::now();
            std::sort(d2.begin(), d2.end());
            t1 = std::chrono::high_resolution_clock::now();
            min_std = std::min(min_std, std::chrono::duration<double, std::milli>(t1 - t0).count());
        }

        std::cout << name << ": std=" << min_std << "ms  r16=" << min_r16 << "ms  (" << (min_std/min_r16) << "x) [" << (ok?"PASS":"FAIL") << "]\n";
    };

    run_bench("Uniform Random",  uniform);
    run_bench("Heavy Duplicates", dupes);
    run_bench("Nearly Sorted",    nearly);

    return 0;
}
