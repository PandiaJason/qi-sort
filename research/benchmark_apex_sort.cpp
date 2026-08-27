#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include "qi_apex_sort.hpp"
#include "../include/qi_radix.hpp"

template <typename Func>
double time_ms(Func f, int iterations = 5) {
    double best = 1e9;
    for (int i = 0; i < iterations; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        f();
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (ms < best) best = ms;
    }
    return best;
}

int main() {
    std::cout << "========================================================================================\n";
    std::cout << "  THE NON-RADIX GRAND CHALLENGE: QI-APEXSORT vs PRODUCTION RADIX QI::SORT\n";
    std::cout << "  1-Pass Memory Streaming (8 MB) vs 3-Pass Radix Engine (24 MB DRAM Traffic)\n";
    std::cout << "  Baseline: qi::sort (Production Radix) = 1.00x\n";
    std::cout << "========================================================================================\n\n";

    std::vector<size_t> sizes = {100000, 1000000, 10000000};
    std::mt19937_64 rng(1337);

    for (size_t N : sizes) {
        std::cout << "--- Dataset Size: N = " << N << " Elements (Uniform Random 32-bit) ---\n";
        std::cout << std::left << std::setw(44) << "Algorithm / Engine"
                  << std::setw(16) << "Time (ms)"
                  << std::setw(24) << "vs qi::sort (Baseline)"
                  << "Status\n";
        std::cout << "----------------------------------------------------------------------------------------\n";

        std::vector<uint32_t> original(N);
        for (size_t i = 0; i < N; ++i) original[i] = rng();

        // 1. qi::sort (PRODUCTION RADIX BASELINE)
        double t_baseline = 0;
        {
            auto data = original;
            t_baseline = time_ms([&]() {
                data = original;
                qi::sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            std::cout << std::left << std::setw(44) << "qi::sort (Production Radix - BASELINE)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_baseline
                      << std::setw(24) << "1.00x (BASELINE)"
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 2. std::sort
        {
            auto data = original;
            double t_std = time_ms([&]() {
                data = original;
                std::sort(data.data(), data.data() + N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double ratio = t_std / t_baseline;
            std::cout << std::left << std::setw(44) << "std::sort (C++ Introsort)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_std
                      << std::setw(24) << (std::to_string(ratio).substr(0, 4) + "x SLOWER")
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 3. QI-ApexSort (Single-Core Non-Radix Challenger)
        {
            auto data = original;
            double t_apex = time_ms([&]() {
                data = original;
                qi_apex::sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double ratio = t_baseline / t_apex;
            std::string label = (ratio >= 1.0) ? (std::to_string(ratio).substr(0, 4) + "x FASTER") 
                                               : (std::to_string(1.0 / ratio).substr(0, 4) + "x slower");
            std::cout << std::left << std::setw(44) << "QI-ApexSort (Single-Core Non-Radix)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_apex
                      << std::setw(24) << label
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 4. QI-ApexSort (Multi-Core Non-Radix Challenger)
        {
            auto data = original;
            double t_apex_par = time_ms([&]() {
                data = original;
                qi_apex::parallel_sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double ratio = t_baseline / t_apex_par;
            std::string label = (ratio >= 1.0) ? (std::to_string(ratio).substr(0, 4) + "x FASTER") 
                                               : (std::to_string(1.0 / ratio).substr(0, 4) + "x slower");
            std::cout << std::left << std::setw(44) << "QI-ApexSort (Multi-Core PARALLEL)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_apex_par
                      << std::setw(24) << label
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 5. qi::parallel_sort (Multi-Core Radix Reference)
        {
            auto data = original;
            double t_radix_par = time_ms([&]() {
                data = original;
                qi::parallel_sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double ratio = t_baseline / t_radix_par;
            std::cout << std::left << std::setw(44) << "qi::parallel_sort (Multi-Core Radix)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_radix_par
                      << std::setw(24) << (std::to_string(ratio).substr(0, 4) + "x FASTER")
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        std::cout << "----------------------------------------------------------------------------------------\n\n";
    }

    return 0;
}
