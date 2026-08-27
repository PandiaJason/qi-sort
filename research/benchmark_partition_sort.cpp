#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include "qi_partition_sort.hpp"
#include "../include/qi_radix.hpp"

template <typename Func>
double time_ms(Func f, int iterations = 3) {
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
    std::cout << "  EXPERIMENTAL NON-RADIX ALGORITHM EVALUATION: QI PARTITION SORT\n";
    std::cout << "  Single-Core & Multi-Core Micro-Bucket Rank Partitioning vs std::sort & Radix Engine\n";
    std::cout << "========================================================================================\n\n";

    std::vector<size_t> sizes = {100000, 1000000, 10000000};
    std::mt19937_64 rng(42);

    for (size_t N : sizes) {
        std::cout << "--- Dataset Size: N = " << N << " Elements ---\n";
        std::cout << std::left << std::setw(36) << "Algorithm / Engine"
                  << std::setw(16) << "Time (ms)"
                  << std::setw(24) << "Speedup vs std::sort"
                  << "Verification\n";
        std::cout << "----------------------------------------------------------------------------------------\n";

        std::vector<uint32_t> original(N);
        for (size_t i = 0; i < N; ++i) original[i] = rng();

        // 1. std::sort
        double t_std = 0;
        {
            auto data = original;
            t_std = time_ms([&]() {
                data = original;
                std::sort(data.data(), data.data() + N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            std::cout << std::left << std::setw(36) << "std::sort (Introsort Baseline)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_std
                      << std::setw(24) << "1.00x (Baseline)"
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 2. QI Partition Sort (Single-Core Non-Radix)
        double t_qpart = 0;
        {
            auto data = original;
            t_qpart = time_ms([&]() {
                data = original;
                qi_partition::sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double speedup = t_std / t_qpart;
            std::cout << std::left << std::setw(36) << "QI Partition Sort (Single-Core)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_qpart
                      << std::setw(24) << (std::to_string(speedup).substr(0, 4) + "x FASTER")
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 3. QI Partition Sort (Multi-Core Parallel Non-Radix)
        double t_qpart_par = 0;
        {
            auto data = original;
            t_qpart_par = time_ms([&]() {
                data = original;
                qi_partition::parallel_sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double speedup = t_std / t_qpart_par;
            std::cout << std::left << std::setw(36) << "QI Partition Sort (Multi-Core PARALLEL)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_qpart_par
                      << std::setw(24) << (std::to_string(speedup).substr(0, 4) + "x FASTER")
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 4. qi::sort (Single-Core Production Radix Engine)
        double t_qradix = 0;
        {
            auto data = original;
            t_qradix = time_ms([&]() {
                data = original;
                qi::sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double speedup = t_std / t_qradix;
            std::cout << std::left << std::setw(36) << "qi::sort (Single-Core Radix)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_qradix
                      << std::setw(24) << (std::to_string(speedup).substr(0, 4) + "x FASTER")
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 5. qi::parallel_sort (Multi-Core Production Radix Engine)
        {
            auto data = original;
            double t_qradix_par = time_ms([&]() {
                data = original;
                qi::parallel_sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double speedup = t_std / t_qradix_par;
            std::cout << std::left << std::setw(36) << "qi::parallel_sort (Multi-Core Radix)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_qradix_par
                      << std::setw(24) << (std::to_string(speedup).substr(0, 4) + "x FASTER")
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        std::cout << "----------------------------------------------------------------------------------------\n\n";
    }

    return 0;
}
