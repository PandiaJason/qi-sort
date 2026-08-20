/*
===============================================================================
REAL GOOGLE VQSORT vs PLAIN RADIX vs QI::SORT (SOURCE-LEVEL BENCHMARK)
===============================================================================
Directly compiled against Google Highway's EXACT production source code:
  - hwy/contrib/sort/vqsort.h (Google Vectorized Quicksort Engine)

Compares Google's actual production SIMD vqsort directly against Plain Radix
variants (Radix-8, Radix-11, Radix-16) and qi::sort across 3,000,000 keys.
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

// Include Google Highway vqsort headers
#include "/tmp/highway/hwy/contrib/sort/vqsort.h"

// Include qi::sort Engine
#include "../include/qi_radix.hpp"

using Clock = std::chrono::high_resolution_clock;

// 1. Google Native SIMD Sorter (vqsort)
static double run_google_vqsort(std::vector<uint32_t>& data) {
    auto start = Clock::now();
    hwy::VQSort(data.data(), data.size(), hwy::SortAscending());
    auto end = Clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// 2. Plain Radix-8
static double run_pradix8(std::vector<uint32_t>& data) {
    auto start = Clock::now();
    qi::detail::radixSort8(data.data(), data.size(), false);
    auto end = Clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// 3. Plain Radix-11
static double run_pradix11(std::vector<uint32_t>& data) {
    auto start = Clock::now();
    qi::detail::radixSort11(data.data(), data.size(), false);
    auto end = Clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// 4. Plain Radix-16
static double run_pradix16(std::vector<uint32_t>& data) {
    auto start = Clock::now();
    qi::detail::radixSort16(data.data(), data.size(), false);
    auto end = Clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// 5. qi::sort (Quantum-Inspired Adaptive Radix Engine)
static double run_qi_sort(std::vector<uint32_t>& data) {
    auto start = Clock::now();
    qi::sort(data);
    auto end = Clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// Dataset Generator
static std::vector<uint32_t> generate_data(const std::string& type, size_t n) {
    std::vector<uint32_t> data(n);
    std::mt19937_64 rng(42);

    if (type == "Uniform Random") {
        std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
        for (size_t i = 0; i < n; ++i) data[i] = dist(rng);
    } else if (type == "Heavy Duplicates") {
        std::uniform_int_distribution<uint32_t> dist(0, 255);
        for (size_t i = 0; i < n; ++i) data[i] = dist(rng);
    } else if (type == "Hash Keys") {
        for (size_t i = 0; i < n; ++i) {
            uint32_t h = static_cast<uint32_t>(i * 2654435761u);
            data[i] = h ^ (h >> 13);
        }
    }
    return data;
}

int main() {
    const size_t N = 3000000; // 3 Million keys
    const std::vector<std::string> distributions = {
        "Uniform Random",
        "Heavy Duplicates",
        "Hash Keys"
    };

    std::cout << "========================================================================================\n";
    std::cout << "  REAL GOOGLE VQSORT vs PLAIN RADIX vs QI::SORT (N = 3,000,000 Keys)\n";
    std::cout << "  Directly compiled against Google Highway source: hwy/contrib/sort/vqsort.h\n";
    std::cout << "========================================================================================\n\n";

    for (const auto& distName : distributions) {
        auto rawData = generate_data(distName, N);

        std::cout << "----------------------------------------------------------------------------------------\n";
        std::cout << " DATASET: " << distName << "\n";
        std::cout << "----------------------------------------------------------------------------------------\n";

        // Untimed warm-ups
        { auto c = rawData; run_google_vqsort(c); }
        { auto c = rawData; run_pradix8(c); }
        { auto c = rawData; run_pradix11(c); }
        { auto c = rawData; run_pradix16(c); }
        { auto c = rawData; run_qi_sort(c); }

        // Timed runs
        auto d_vq  = rawData; double t_vq  = run_google_vqsort(d_vq);
        auto d_r8  = rawData; double t_r8  = run_pradix8(d_r8);
        auto d_r11 = rawData; double t_r11 = run_pradix11(d_r11);
        auto d_r16 = rawData; double t_r16 = run_pradix16(d_r16);
        auto d_qi  = rawData; double t_qi  = run_qi_sort(d_qi);

        auto row = [&](const std::string& name, double t) {
            double vs_vq = t_vq / t;
            std::cout << "  " << std::left << std::setw(36) << name
                      << std::setw(12) << std::fixed << std::setprecision(2) << t << " ms  "
                      << std::setw(16) << (std::to_string(vs_vq).substr(0, 4) + "x vs vqsort")
                      << "\n";
        };

        row("Google Native (vqsort)",            t_vq);
        row("Plain Radix-8  (Fixed 4-Pass)",     t_r8);
        row("Plain Radix-11 (Fixed 3-Pass)",     t_r11);
        row("Plain Radix-16 (Fixed 2-Pass)",     t_r16);
        row("qi::sort (Adaptive Engine)",        t_qi);

        std::cout << "  --> qi::sort Speedup vs Google vqsort: "
                  << std::setprecision(2) << (t_vq / t_qi) << "x FASTER  |  vs Plain Radix-16: "
                  << (t_r16 / t_qi) << "x FASTER\n\n";
    }

    std::cout << "========================================================================================\n";
    return 0;
}
