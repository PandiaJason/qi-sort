/*
================================================================
REAL QUICKSORT vs QI-SORT BENCHMARK
================================================================
Tests:
  1. C Standard Library qsort() (stdlib.h qsort)
  2. Native Hoare Quicksort (median-of-3 pivot + insertion sort cutoff)
  3. C++ std::sort (libstdc++/libc++ Introsort — highly optimized Quicksort variant)
  4. qi::sort (Our Engine)

On both Real Datasets and Uniform Random Datasets.
================================================================
*/

#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include "../include/qi_radix.hpp"

using namespace std;
using Clock = chrono::high_resolution_clock;

// C qsort callback
static int compare_u32(const void* a, const void* b) {
    uint32_t arg1 = *static_cast<const uint32_t*>(a);
    uint32_t arg2 = *static_cast<const uint32_t*>(b);
    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

// Native Hoare Quicksort
static void hoare_quicksort(uint32_t* arr, int low, int high) {
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
    hoare_quicksort(arr, low, j);
    hoare_quicksort(arr, j + 1, high);
}

static inline double ms(function<void()> fn) {
    auto t = Clock::now();
    fn();
    return chrono::duration<double, milli>(Clock::now() - t).count();
}

int main() {
    cout << "\n================================================================\n";
    cout << "  REAL QUICKSORT (C qsort, Hoare Quicksort, std::sort) vs QI-SORT\n";
    cout << "================================================================\n\n";

    struct BenchmarkDataset {
        string name;
        vector<uint32_t> data;
    };

    vector<BenchmarkDataset> datasets;

    // 1. Uniform Random 1M
    {
        vector<uint32_t> d(1000000);
        mt19937_64 rng(42);
        uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
        for (auto& x : d) x = dist(rng);
        datasets.push_back({"1M Random 32-bit Integers", move(d)});
    }

    // 2. Uniform Random 5M
    {
        vector<uint32_t> d(5000000);
        mt19937_64 rng(123);
        uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
        for (auto& x : d) x = dist(rng);
        datasets.push_back({"5M Random 32-bit Integers", move(d)});
    }

    // 3. NYC Taxi CSV sample (if file exists)
    {
        ifstream f("/tmp/nyc_taxi_sample.csv");
        if (f) {
            vector<uint32_t> d;
            string line;
            getline(f, line); // header
            struct tm tm{};
            while (getline(f, line)) {
                istringstream ss(line);
                string tok;
                if (!getline(ss, tok, ',')) continue;
                if (!getline(ss, tok, ',')) continue;
                int mo=0,dy=0,yr=0,hr=0,mn=0,sc=0; char ampm[4]="AM";
                if (sscanf(tok.c_str(), "%d/%d/%d %d:%d:%d %3s", &mo,&dy,&yr,&hr,&mn,&sc,ampm) == 7) {
                    if (strcmp(ampm,"PM")==0 && hr!=12) hr+=12;
                    if (strcmp(ampm,"AM")==0 && hr==12) hr=0;
                    tm.tm_year=yr-1900; tm.tm_mon=mo-1; tm.tm_mday=dy; tm.tm_hour=hr; tm.tm_min=mn; tm.tm_sec=sc;
                    time_t t = mktime(&tm);
                    if (t > 0) d.push_back((uint32_t)t);
                }
            }
            if (!d.empty()) datasets.push_back({"NYC Taxi Timestamps (87k)", move(d)});
        }
    }

    for (auto& ds : datasets) {
        cout << "Dataset: " << ds.name << " (N = " << ds.data.size() << ")\n";

        // C stdlib qsort
        auto d_cqsort = ds.data;
        double t_cqsort = ms([&]() {
            qsort(d_cqsort.data(), d_cqsort.size(), sizeof(uint32_t), compare_u32);
        });

        // Hoare Quicksort
        auto d_hoare = ds.data;
        double t_hoare = ms([&]() {
            hoare_quicksort(d_hoare.data(), 0, (int)d_hoare.size() - 1);
        });

        // std::sort (C++ Introsort / optimized Quicksort)
        auto d_std = ds.data;
        double t_std = ms([&]() {
            sort(d_std.begin(), d_std.end());
        });

        // qi::sort
        auto d_qi = ds.data;
        double t_qi = ms([&]() {
            qi::sort(d_qi);
        });

        // Verification
        bool pass = (d_qi == d_std) && (d_cqsort == d_std) && (d_hoare == d_std);

        cout << left << setw(35) << "Algorithm"
             << setw(16) << "Time (ms)"
             << setw(20) << "vs C qsort()"
             << "vs std::sort\n";
        cout << string(80, '-') << "\n";

        cout << setw(35) << "C stdlib qsort()" 
             << setw(16) << fixed << setprecision(2) << t_cqsort 
             << setw(20) << "1.00x (Baseline)" 
             << to_string(t_std / t_cqsort).substr(0,4) + "x" << "\n";

        cout << setw(35) << "Native Hoare Quicksort" 
             << setw(16) << t_hoare 
             << setw(20) << to_string(t_cqsort / t_hoare).substr(0,4) + "x faster" 
             << to_string(t_std / t_hoare).substr(0,4) + "x" << "\n";

        cout << setw(35) << "std::sort (C++ Introsort)" 
             << setw(16) << t_std 
             << setw(20) << to_string(t_cqsort / t_std).substr(0,4) + "x faster" 
             << "1.00x (Baseline)" << "\n";

        cout << setw(35) << "qi::sort (Our Engine)" 
             << setw(16) << t_qi 
             << setw(20) << to_string(t_cqsort / t_qi).substr(0,4) + "x FASTER" 
             << to_string(t_std / t_qi).substr(0,4) + "x FASTER" << "\n";

        cout << string(80, '-') << "\n";
        cout << "Correctness: " << (pass ? "PASS (100% Match)" : "FAIL") << "\n\n";
    }

    return 0;
}
