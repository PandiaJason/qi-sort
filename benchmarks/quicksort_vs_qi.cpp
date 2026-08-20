/*
================================================================
BENCHMARK: qi::sort vs Quicksort (Native C++)
================================================================
Compares:
  1. Traditional Quicksort (Hoare partition scheme with median-of-three pivot)
  2. std::sort (C++ Introsort — QuickSort + HeapSort fallback + InsertionSort)
  3. qi::sort (Quantum-Inspired Radix Engine)

Across dataset sizes: 100K, 1M, 10M
================================================================
*/

#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>
#include <iomanip>
#include <numeric>
#include "../include/qi_radix.hpp"

using namespace std;
using Clock = chrono::high_resolution_clock;

// ─── Native Classic Quicksort Implementation ─────────────────────────────────
static void quicksort_classic(uint32_t* arr, int low, int high) {
    if (low >= high) return;

    // Small array threshold -> Insertion sort
    if (high - low < 16) {
        for (int i = low + 1; i <= high; ++i) {
            uint32_t key = arr[i];
            int j = i - 1;
            while (j >= low && arr[j] > key) {
                arr[j + 1] = arr[j];
                j--;
            }
            arr[j + 1] = key;
        }
        return;
    }

    // Median-of-three pivot selection
    int mid = low + (high - low) / 2;
    if (arr[mid] < arr[low]) swap(arr[mid], arr[low]);
    if (arr[high] < arr[low]) swap(arr[high], arr[low]);
    if (arr[high] < arr[mid]) swap(arr[high], arr[mid]);

    uint32_t pivot = arr[mid];
    int i = low - 1;
    int j = high + 1;

    // Hoare partitioning
    while (true) {
        do { i++; } while (arr[i] < pivot);
        do { j--; } while (arr[j] > pivot);
        if (i >= j) break;
        swap(arr[i], arr[j]);
    }

    quicksort_classic(arr, low, j);
    quicksort_classic(arr, j + 1, high);
}

static inline double ms(function<void()> fn) {
    auto t = Clock::now();
    fn();
    return chrono::duration<double, milli>(Clock::now() - t).count();
}

int main() {
    cout << "\n================================================================\n";
    cout << "  qi::sort vs QUICKSORT BENCHMARK (Native C++ -O3 -march=native)\n";
    cout << "================================================================\n\n";

    vector<size_t> sizes = {100000, 1000000, 10000000};
    mt19937_64 rng(42);

    for (size_t N : sizes) {
        cout << "--- Dataset Size: N = " << N << " ---" << endl;

        vector<uint32_t> orig(N);
        uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
        for (auto& x : orig) x = dist(rng);

        // 1. Classic Quicksort
        auto d_qs = orig;
        double t_qs = ms([&]() { quicksort_classic(d_qs.data(), 0, (int)N - 1); });

        // 2. std::sort (Introsort = Quicksort variant)
        auto d_std = orig;
        double t_std = ms([&]() { sort(d_std.begin(), d_std.end()); });

        // 3. qi::sort
        auto d_qi = orig;
        double t_qi = ms([&]() { qi::sort(d_qi); });

        // Verify all outputs
        bool qs_ok = is_sorted(d_qs.begin(), d_qs.end());
        bool std_ok = is_sorted(d_std.begin(), d_std.end());
        bool qi_ok = (d_qi == d_std);

        cout << left << setw(28) << "Algorithm" 
             << setw(16) << "Time (ms)" 
             << setw(20) << "vs Classic QuickSort" 
             << "Status\n";
        cout << string(70, '-') << endl;

        cout << setw(28) << "Classic Quicksort" 
             << setw(16) << fixed << setprecision(2) << t_qs 
             << setw(20) << "1.00x (Baseline)" 
             << (qs_ok ? "PASS" : "FAIL") << "\n";

        cout << setw(28) << "std::sort (Introsort)" 
             << setw(16) << t_std 
             << setw(20) << to_string(t_qs / t_std).substr(0,4) + "x faster" 
             << (std_ok ? "PASS" : "FAIL") << "\n";

        cout << setw(28) << "qi::sort (Our Engine)" 
             << setw(16) << t_qi 
             << setw(20) << to_string(t_qs / t_qi).substr(0,4) + "x FASTER" 
             << (qi_ok ? "PASS" : "FAIL") << "\n";

        cout << string(70, '-') << "\n";
        cout << "qi::sort Speedup vs std::sort : " << setprecision(2) << (t_std / t_qi) << "x FASTER\n\n";
    }

    return 0;
}
