#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include "qi_field_sort.hpp"
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
    std::cout << "  NEW ORIGINAL ALGORITHM: QI-FIELDSORT (Continuous Density-Field Rank Inversion)\n";
    std::cout << "  100% NON-RADIX, Zero-Bit-Shift, Continuous Potential-Well Inversion Engine\n";
    std::cout << "========================================================================================\n\n";

    std::vector<size_t> sizes = {100000, 1000000, 10000000};
    std::mt19937_64 rng(1337);

    for (size_t N : sizes) {
        std::cout << "--- Dataset Size: N = " << N << " Elements (Uniform Random 32-bit) ---\n";
        std::cout << std::left << std::setw(44) << "Algorithm / Engine"
                  << std::setw(16) << "Time (ms)"
                  << std::setw(24) << "Speedup vs std::sort"
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
            std::cout << std::left << std::setw(44) << "std::sort (C++ Introsort Baseline)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_std
                      << std::setw(24) << "1.00x (Baseline)"
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 2. QI-FieldSort (Single-Core Non-Radix)
        double t_field = 0;
        {
            auto data = original;
            t_field = time_ms([&]() {
                data = original;
                qi_field::sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double speedup = t_std / t_field;
            std::cout << std::left << std::setw(44) << "QI-FieldSort (Single-Core Non-Radix)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_field
                      << std::setw(24) << (std::to_string(speedup).substr(0, 4) + "x FASTER")
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 3. QI-FieldSort (Multi-Core Parallel Non-Radix)
        double t_field_par = 0;
        {
            auto data = original;
            t_field_par = time_ms([&]() {
                data = original;
                qi_field::parallel_sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double speedup = t_std / t_field_par;
            std::cout << std::left << std::setw(44) << "QI-FieldSort (Multi-Core PARALLEL)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_field_par
                      << std::setw(24) << (std::to_string(speedup).substr(0, 4) + "x FASTER")
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 4. qi::sort (Single-Core Radix Baseline Reference)
        {
            auto data = original;
            double t_radix = time_ms([&]() {
                data = original;
                qi::sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double speedup = t_std / t_radix;
            std::cout << std::left << std::setw(44) << "qi::sort (Production Radix Single-Core)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_radix
                      << std::setw(24) << (std::to_string(speedup).substr(0, 4) + "x FASTER")
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        std::cout << "----------------------------------------------------------------------------------------\n\n";
    }

    return 0;
}
