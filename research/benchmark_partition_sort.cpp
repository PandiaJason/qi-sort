#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include "qi_partition_sort.hpp"
#include "../include/qi_radix.hpp"

// Classic QuickSort for baseline
void classicQuickSort(uint32_t* arr, int low, int high) {
    if (low < high) {
        uint32_t pivot = arr[high];
        int i = (low - 1);
        for (int j = low; j <= high - 1; j++) {
            if (arr[j] < pivot) {
                i++;
                std::swap(arr[i], arr[j]);
            }
        }
        std::swap(arr[i + 1], arr[high]);
        int pi = i + 1;
        classicQuickSort(arr, low, pi - 1);
        classicQuickSort(arr, pi + 1, high);
    }
}

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
    std::cout << "  Mechanism: Empirical Quantile Sampling + L1-Resident Multiway Partitioning\n";
    std::cout << "========================================================================================\n\n";

    std::vector<size_t> sizes = {100000, 1000000, 5000000};

    std::mt19937_64 rng(42);

    for (size_t N : sizes) {
        std::cout << "--- Dataset Size: N = " << N << " Elements ---\n";
        std::cout << std::left << std::setw(32) << "Algorithm"
                  << std::setw(16) << "Time (ms)"
                  << std::setw(24) << "Speedup vs std::sort"
                  << "Verification\n";
        std::cout << "----------------------------------------------------------------------------------------\n";

        // Generate datasets
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
            std::cout << std::left << std::setw(32) << "std::sort (Introsort)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_std
                      << std::setw(24) << "1.00x (Baseline)"
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 2. Classic Quicksort (only on smaller sizes)
        if (N <= 1000000) {
            auto data = original;
            double t_qs = time_ms([&]() {
                data = original;
                classicQuickSort(data.data(), 0, N - 1);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            std::cout << std::left << std::setw(32) << "Classic Hoare Quicksort"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_qs
                      << std::setw(24) << (std::to_string(t_std / t_qs).substr(0, 4) + "x")
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 3. QI Partition Sort (Experimental Non-Radix)
        double t_qpart = 0;
        {
            auto data = original;
            t_qpart = time_ms([&]() {
                data = original;
                qi_partition::sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double speedup = t_std / t_qpart;
            std::cout << std::left << std::setw(32) << "QI Partition Sort (Non-Radix)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_qpart
                      << std::setw(24) << (std::to_string(speedup).substr(0, 4) + "x FASTER")
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 4. Reference: qi::sort (Production Adaptive Radix baseline)
        {
            auto data = original;
            double t_qradix = time_ms([&]() {
                data = original;
                qi::sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double speedup = t_std / t_qradix;
            std::cout << std::left << std::setw(32) << "qi::sort (Radix Engine Ref)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_qradix
                      << std::setw(24) << (std::to_string(speedup).substr(0, 4) + "x FASTER")
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        std::cout << "----------------------------------------------------------------------------------------\n\n";
    }

    return 0;
}
