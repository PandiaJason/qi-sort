/*
===============================================================================
HEAD-TO-HEAD BENCHMARK: ORIGINAL QI::SORT vs NEW ENHANCED QI::SORT
===============================================================================
Compares the Original version of qi::sort directly against the New version of qi::sort
across 3,000,000 keys across 4 distinct data distributions.
===============================================================================
*/

#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>
#include <iomanip>
#include <cstring>
#include <cmath>
#include <cstdint>

// Include current header-only qi::sort engine
#include "../include/qi_radix.hpp"

using Clock = std::chrono::high_resolution_clock;

// ── ORIGINAL QI::SORT SCALAR RADIX KERNELS (FROM INITIAL CODEBASE) ──
namespace qi_original {

inline void original_radixSort8(uint32_t* data, size_t n) {
    if (n <= 1) return;
    if (std::is_sorted(data, data + n)) return;

    std::vector<uint32_t> buffer(n);
    uint32_t* src = data;
    uint32_t* dst = buffer.data();

    for (int pass = 0; pass < 4; ++pass) {
        size_t count[256] = {0};
        int shift = pass * 8;
        for (size_t i = 0; i < n; ++i) count[(src[i] >> shift) & 0xFF]++;
        size_t total = 0;
        for (int i = 0; i < 256; ++i) { size_t c = count[i]; count[i] = total; total += c; }
        if (count[0] == n) continue;
        for (size_t i = 0; i < n; ++i) {
            uint8_t byte = (src[i] >> shift) & 0xFF;
            dst[count[byte]++] = src[i];
        }
        std::swap(src, dst);
    }
    if (src != data) std::memcpy(data, src, n * sizeof(uint32_t));
}

inline void original_radixSort11(uint32_t* data, size_t n) {
    if (n <= 1) return;
    if (std::is_sorted(data, data + n)) return;

    std::vector<uint32_t> buffer(n);
    uint32_t* src = data;
    uint32_t* dst = buffer.data();

    const int shifts[3] = {0, 11, 22};
    const size_t masks[3] = {0x7FF, 0x7FF, 0x3FF};
    const int numBuckets[3] = {2048, 2048, 1024};

    for (int pass = 0; pass < 3; ++pass) {
        int shift = shifts[pass];
        size_t mask = masks[pass];
        int buckets = numBuckets[pass];
        std::vector<size_t> count(buckets, 0);

        for (size_t i = 0; i < n; ++i) count[(src[i] >> shift) & mask]++;
        size_t total = 0;
        for (int i = 0; i < buckets; ++i) { size_t c = count[i]; count[i] = total; total += c; }
        if (count[0] == n) continue;
        for (size_t i = 0; i < n; ++i) {
            size_t bucket = (src[i] >> shift) & mask;
            dst[count[bucket]++] = src[i];
        }
        std::swap(src, dst);
    }
    if (src != data) std::memcpy(data, src, n * sizeof(uint32_t));
}

inline void original_radixSort16(uint32_t* data, size_t n) {
    if (n <= 1) return;
    if (std::is_sorted(data, data + n)) return;

    std::vector<uint32_t> buffer(n);
    uint32_t* src = data;
    uint32_t* dst = buffer.data();

    for (int pass = 0; pass < 2; ++pass) {
        std::vector<size_t> count(65536, 0);
        int shift = pass * 16;
        for (size_t i = 0; i < n; ++i) count[(src[i] >> shift) & 0xFFFF]++;
        size_t total = 0;
        for (int i = 0; i < 65536; ++i) { size_t c = count[i]; count[i] = total; total += c; }
        if (count[0] == n) continue;
        for (size_t i = 0; i < n; ++i) {
            uint16_t key16 = (src[i] >> shift) & 0xFFFF;
            dst[count[key16]++] = src[i];
        }
        std::swap(src, dst);
    }
    if (src != data) std::memcpy(data, src, n * sizeof(uint32_t));
}

inline void original_sort(uint32_t* data, size_t n) {
    if (n <= 1) return;
    auto state = qi::analyze(data, n);
    switch (state.recommendedRadix) {
        case qi::Radix::R8:  original_radixSort8(data, n); break;
        case qi::Radix::R11: original_radixSort11(data, n); break;
        case qi::Radix::R16: original_radixSort16(data, n); break;
    }
}

} // namespace qi_original

// Dataset Generator
static std::vector<uint32_t> generate_data(const std::string& type, size_t n) {
    std::vector<uint32_t> data(n);
    std::mt19937_64 rng(42);

    if (type == "1. Uniform Random") {
        std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
        for (size_t i = 0; i < n; ++i) data[i] = dist(rng);
    } else if (type == "2. Heavy Duplicates") {
        std::uniform_int_distribution<uint32_t> dist(0, 255);
        for (size_t i = 0; i < n; ++i) data[i] = dist(rng);
    } else if (type == "3. Hash Keys") {
        for (size_t i = 0; i < n; ++i) {
            uint32_t h = static_cast<uint32_t>(i * 2654435761u);
            data[i] = h ^ (h >> 13);
        }
    } else if (type == "4. Nearly Sorted (95%)") {
        for (size_t i = 0; i < n; ++i) data[i] = static_cast<uint32_t>(i);
        size_t swaps = n / 100;
        std::uniform_int_distribution<size_t> dist(0, n - 1);
        for (size_t i = 0; i < swaps; ++i) std::swap(data[dist(rng)], data[dist(rng)]);
    }
    return data;
}

int main() {
    const size_t N = 3000000; // 3 Million keys
    const int TRIALS = 5;
    const std::vector<std::string> distributions = {
        "1. Uniform Random",
        "2. Heavy Duplicates",
        "3. Hash Keys",
        "4. Nearly Sorted (95%)"
    };

    std::cout << "========================================================================================\n";
    std::cout << "  HEAD-TO-HEAD BENCHMARK: ORIGINAL QI::SORT vs NEW ENHANCED QI::SORT (N = 3,000,000 Keys)\n";
    std::cout << "========================================================================================\n\n";

    for (const auto& distName : distributions) {
        auto rawData = generate_data(distName, N);

        double total_orig = 0.0;
        double total_new = 0.0;

        for (int t = 0; t < TRIALS; ++t) {
            // Original
            {
                auto c = rawData;
                auto start = Clock::now();
                qi_original::original_sort(c.data(), c.size());
                auto end = Clock::now();
                total_orig += std::chrono::duration<double, std::milli>(end - start).count();
            }

            // New
            {
                auto c = rawData;
                auto start = Clock::now();
                qi::sort(c.data(), c.size());
                auto end = Clock::now();
                total_new += std::chrono::duration<double, std::milli>(end - start).count();
            }
        }

        double avg_orig = total_orig / TRIALS;
        double avg_new  = total_new / TRIALS;
        double diff_pct = ((avg_orig - avg_new) / avg_orig) * 100.0;

        std::cout << "----------------------------------------------------------------------------------------\n";
        std::cout << " DATASET: " << distName << " (5-Trial Average)\n";
        std::cout << "----------------------------------------------------------------------------------------\n";
        std::cout << "  Original qi::sort Engine: " << std::fixed << std::setprecision(2) << avg_orig << " ms  ("
                  << (int)(N / avg_orig / 1000.0) << " MKeys/s)\n";
        std::cout << "  New Enhanced qi::sort:    " << std::fixed << std::setprecision(2) << avg_new << " ms  ("
                  << (int)(N / avg_new / 1000.0) << " MKeys/s)\n";

        if (avg_new < avg_orig) {
            std::cout << "  --> WINNER: New Version is " << std::setprecision(2) << std::abs(diff_pct)
                      << "% FASTER! (" << (avg_orig / avg_new) << "x speedup)\n\n";
        } else {
            std::cout << "  --> RESULT: Both engines perform virtually identical (~"
                      << std::setprecision(2) << std::abs(diff_pct) << "% difference)\n\n";
        }
    }

    std::cout << "========================================================================================\n";
    return 0;
}
