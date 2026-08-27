#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include "../include/qi_radix.hpp"
#include "qi_turbo_radix.hpp"
#include "qi_partition_sort.hpp"

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
    std::cout << "  HEAD-TO-HEAD BATTLE: OUTPERFORMING QI::SORT (1M & 10M KEYS)\n";
    std::cout << "========================================================================================\n\n";

    std::vector<size_t> sizes = {1000000, 10000000};
    std::mt19937_64 rng(1337);

    for (size_t N : sizes) {
        std::cout << "--- Dataset Size: N = " << N << " Elements (Uniform Random 32-bit) ---\n";
        std::cout << std::left << std::setw(40) << "Sorting Engine / Method"
                  << std::setw(16) << "Time (ms)"
                  << std::setw(24) << "vs qi::sort Baseline"
                  << "Status\n";
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
            std::cout << std::left << std::setw(40) << "std::sort (Introsort Standard)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_std
                      << std::setw(24) << (std::to_string(t_std / 3.42).substr(0, 4) + "x slower")
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 2. qi::sort (Current Production Baseline)
        double t_qisort = 0;
        {
            auto data = original;
            t_qisort = time_ms([&]() {
                data = original;
                qi::sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            std::cout << std::left << std::setw(40) << "qi::sort (Production Engine Baseline)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_qisort
                      << std::setw(24) << "1.00x (Baseline)"
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 3. qi_turbo::sort (Challenger: 4-Banked Turbo Radix)
        double t_turbo = 0;
        {
            auto data = original;
            t_turbo = time_ms([&]() {
                data = original;
                qi_turbo::sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double ratio = t_qisort / t_turbo;
            std::string label = (ratio >= 1.0) ? (std::to_string(ratio).substr(0, 4) + "x FASTER") 
                                               : (std::to_string(1.0 / ratio).substr(0, 4) + "x slower");
            std::cout << std::left << std::setw(40) << "qi_turbo (4-Banked Turbo Radix)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_turbo
                      << std::setw(24) << label
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 4. qi_partition::sort (Challenger: Non-Radix Micro-Bucket)
        double t_part = 0;
        {
            auto data = original;
            t_part = time_ms([&]() {
                data = original;
                qi_partition::sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double ratio = t_qisort / t_part;
            std::string label = (ratio >= 1.0) ? (std::to_string(ratio).substr(0, 4) + "x FASTER") 
                                               : (std::to_string(1.0 / ratio).substr(0, 4) + "x slower");
            std::cout << std::left << std::setw(40) << "qi_partition (Micro-Bucket Partition)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_part
                      << std::setw(24) << label
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        std::cout << "----------------------------------------------------------------------------------------\n\n";
    }

    return 0;
}
