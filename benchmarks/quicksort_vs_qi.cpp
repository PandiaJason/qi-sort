/*
================================================================
BENCHMARK: Classic Quicksort vs std::sort vs Plain Radix vs qi::sort
================================================================
Compares:
  1. Classic Quicksort (Hoare partition scheme with median-of-three pivot)
  2. std::sort (C++ Introsort)
  3. Plain Radix-8 (Fixed 4-Pass)
  4. Plain Radix-11 (Fixed 3-Pass)
  5. Plain Radix-16 (Fixed 2-Pass)
  6. qi::sort (Our Adaptive Radix Engine)

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
#include <functional>
#include "../include/qi_radix.hpp"

using namespace std;
using Clock = chrono::high_resolution_clock;

// ─── Native Classic Quicksort Implementation ─────────────────────────────────
static void quicksort_classic(uint32_t* arr, int low, int high) {
    if (low >= high) return;

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

    int mid = low + (high - low) / 2;
    if (arr[mid] < arr[low]) swap(arr[mid], arr[low]);
    if (arr[high] < arr[low]) swap(arr[high], arr[low]);
    if (arr[high] < arr[mid]) swap(arr[high], arr[mid]);

    uint32_t pivot = arr[mid];
    int i = low - 1;
    int j = high + 1;

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
    cout << "\n========================================================================================\n";
    cout << "  QUICKSORT vs STD::SORT vs PLAIN RADIX (8, 11, 16) vs QI::SORT\n";
    cout << "========================================================================================\n\n";

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

        // 2. std::sort
        auto d_std = orig;
        double t_std = ms([&]() { sort(d_std.begin(), d_std.end()); });

        // 3. Plain Radix-8
        auto d_r8 = orig;
        double t_r8 = ms([&]() { qi::radix_8(d_r8.data(), N); });

        // 4. Plain Radix-11
        auto d_r11 = orig;
        double t_r11 = ms([&]() { qi::radix_11(d_r11.data(), N); });

        // 5. Plain Radix-16
        auto d_r16 = orig;
        double t_r16 = ms([&]() { qi::radix_16(d_r16.data(), N); });

        // 6. qi::sort
        auto d_qi = orig;
        double t_qi = ms([&]() { qi::sort(d_qi); });

        bool qs_ok = is_sorted(d_qs.begin(), d_qs.end());
        bool std_ok = is_sorted(d_std.begin(), d_std.end());
        bool r8_ok = (d_r8 == d_std);
        bool r11_ok = (d_r11 == d_std);
        bool r16_ok = (d_r16 == d_std);
        bool qi_ok = (d_qi == d_std);

        cout << left << setw(30) << "Algorithm" 
             << setw(16) << "Time (ms)" 
             << setw(22) << "vs Classic QuickSort" 
             << "Status\n";
        cout << string(76, '-') << endl;

        cout << setw(30) << "Classic Quicksort" 
             << setw(16) << fixed << setprecision(2) << t_qs 
             << setw(22) << "1.00x (Baseline)" 
             << (qs_ok ? "PASS" : "FAIL") << "\n";

        cout << setw(30) << "std::sort (Introsort)" 
             << setw(16) << t_std 
             << setw(22) << to_string(t_qs / t_std).substr(0,4) + "x faster" 
             << (std_ok ? "PASS" : "FAIL") << "\n";

        cout << setw(30) << "Plain Radix-8 (4-Pass)" 
             << setw(16) << t_r8 
             << setw(22) << to_string(t_qs / t_r8).substr(0,4) + "x faster" 
             << (r8_ok ? "PASS" : "FAIL") << "\n";

        cout << setw(30) << "Plain Radix-11 (3-Pass)" 
             << setw(16) << t_r11 
             << setw(22) << to_string(t_qs / t_r11).substr(0,4) + "x faster" 
             << (r11_ok ? "PASS" : "FAIL") << "\n";

        cout << setw(30) << "Plain Radix-16 (2-Pass)" 
             << setw(16) << t_r16 
             << setw(22) << to_string(t_qs / t_r16).substr(0,4) + "x faster" 
             << (r16_ok ? "PASS" : "FAIL") << "\n";

        cout << setw(30) << "qi::sort (Adaptive Engine)" 
             << setw(16) << t_qi 
             << setw(22) << to_string(t_qs / t_qi).substr(0,4) + "x FASTER" 
             << (qi_ok ? "PASS" : "FAIL") << "\n";

        cout << string(76, '-') << "\n";
        cout << "qi::sort Speedup vs std::sort : " << setprecision(2) << (t_std / t_qi) << "x FASTER  |  vs Plain Radix-16 : " << (t_r16 / t_qi) << "x FASTER\n\n";
    }

    return 0;
}
