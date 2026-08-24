#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <cstdint>

using u32 = uint32_t;

// Radix-8 with 4-element L1-resident write buffers (5 KB total memory footprint)
inline void radixSort8_l1(u32* data, size_t n) {
    if (n <= 1) return;
    std::vector<u32> scratch(n);
    u32* buf = scratch.data();
    u32* src = data;
    u32* dst = buf;

    // 4-element L1 buffers per bucket: 256 * 16 bytes = 4 KB
    alignas(64) u32 wbuf[256][4];
    alignas(64) uint8_t wcnt[256];

    for (int pass = 0; pass < 4; ++pass) {
        int shift = pass * 8;
        alignas(64) uint32_t count[256] = {};

        // Combined count pass (1 KB)
        for (size_t i = 0; i < n; ++i) {
            count[(src[i] >> shift) & 0xFF]++;
        }

        // Prefix sum
        uint32_t sum = 0;
        for (int i = 0; i < 256; ++i) {
            uint32_t c = count[i];
            count[i] = sum;
            sum += c;
        }

        // Skip pass if all elements hit bucket 0
        if (count[0] == n) continue;

        // Block-buffered scatter: 4-element buffer flush (16 bytes)
        std::memset(wcnt, 0, 256);
        for (size_t i = 0; i < n; ++i) {
            u32 v = src[i];
            uint8_t b = (v >> shift) & 0xFF;
            wbuf[b][wcnt[b]++] = v;
            if (wcnt[b] == 4) {
                std::memcpy(&dst[count[b]], wbuf[b], 16);
                count[b] += 4;
                wcnt[b] = 0;
            }
        }
        for (int b = 0; b < 256; ++b) {
            if (wcnt[b] > 0) {
                std::memcpy(&dst[count[b]], wbuf[b], wcnt[b] * sizeof(u32));
                count[b] += wcnt[b];
                wcnt[b] = 0;
            }
        }
        std::swap(src, dst);
    }

    if (src != data) std::memcpy(data, src, n * sizeof(u32));
}

int main() {
    constexpr size_t N = 2'000'000;
    std::vector<uint32_t> data(N);
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFFu);

    for (size_t i = 0; i < N; ++i) data[i] = dist(rng);

    auto test_data = data;
    radixSort8_l1(test_data.data(), N);
    bool ok = std::is_sorted(test_data.begin(), test_data.end());
    std::cout << "[radixSort8_l1] Correctness: " << (ok ? "PASS" : "FAIL") << "\n";

    double min_l1 = 1e9, min_std = 1e9;
    for (int run = 0; run < 5; ++run) {
        auto d1 = data;
        auto t0 = std::chrono::high_resolution_clock::now();
        radixSort8_l1(d1.data(), N);
        auto t1 = std::chrono::high_resolution_clock::now();
        double dt1 = std::chrono::duration<double, std::milli>(t1 - t0).count();
        min_l1 = std::min(min_l1, dt1);

        auto d2 = data;
        t0 = std::chrono::high_resolution_clock::now();
        std::sort(d2.begin(), d2.end());
        t1 = std::chrono::high_resolution_clock::now();
        double dt2 = std::chrono::duration<double, std::milli>(t1 - t0).count();
        min_std = std::min(min_std, dt2);
    }

    std::cout << "std::sort:       " << min_std << " ms\n";
    std::cout << "radixSort8_l1:   " << min_l1 << " ms  (" << (min_std / min_l1) << "x speedup)\n";

    return 0;
}
