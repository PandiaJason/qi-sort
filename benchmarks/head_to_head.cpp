/*
================================================================
REAL HEAD-TO-HEAD: qi::sort vs Best-Known C++ Sorting Libraries
================================================================
Competitors (all native C++, same compiler, same machine):

  1. std::sort          — C++ standard library Introsort (GCC/Clang)
  2. pdqsort            — Pattern-Defeating QuickSort by Orson Peters
                          (used in Rust's standard library)
  3. ska_sort           — Malte Skarupke's American Flag / Radix Sort
                          (widely cited as one of the fastest radix sorts)
  4. qi::sort (scalar)  — Our engine, single-threaded
  5. qi::sort (parallel)— Our engine, multi-threaded

Datasets:
  A. Uniform random 32-bit  (worst case for comparison sorts)
  B. Nearly sorted           (best case for adaptive sorts)
  C. Few unique values       (stress test for radix bucket waste)
  D. Pipe organ pattern      (adversarial for naive quicksort)
  E. Random with range 0-65535 (medium entropy)
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
#include <cstring>

// Competitor headers
#include "/tmp/pdqsort.h"
#include "/tmp/ska_sort.hpp"

// Our header
#include "../include/qi_radix.hpp"

using namespace std;
using Clock = chrono::high_resolution_clock;

static inline double bench(function<void()> fn, int runs = 3) {
    double best = 1e18;
    for (int r = 0; r < runs; r++) {
        auto t = Clock::now();
        fn();
        double ms = chrono::duration<double, milli>(Clock::now() - t).count();
        best = min(best, ms);
    }
    return best;
}

struct Dataset {
    string name;
    vector<uint32_t> data;
};

int main() {
    const size_t N = 10000000;
    unsigned int cores = thread::hardware_concurrency();

    cout << "\n================================================================\n";
    cout << "  HEAD-TO-HEAD: qi::sort vs Best-Known C++ Sort Libraries\n";
    cout << "  N = " << N << " | Cores = " << cores
         << " | g++ -O3 -march=native | Best of 3 runs\n";
    cout << "================================================================\n";

    // Generate datasets
    vector<Dataset> datasets;

    // A. Uniform random
    {
        mt19937_64 rng(42);
        vector<uint32_t> d(N);
        uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
        for (auto& x : d) x = dist(rng);
        datasets.push_back({"Uniform Random 32-bit", std::move(d)});
    }

    // B. Nearly sorted (95% sorted, 5% random swaps)
    {
        vector<uint32_t> d(N);
        iota(d.begin(), d.end(), 0);
        mt19937_64 rng(123);
        for (size_t i = 0; i < N / 20; i++) {
            size_t a = rng() % N, b = rng() % N;
            swap(d[a], d[b]);
        }
        datasets.push_back({"Nearly Sorted (95%)", std::move(d)});
    }

    // C. Few unique values (1000 unique)
    {
        mt19937_64 rng(999);
        vector<uint32_t> d(N);
        uniform_int_distribution<uint32_t> dist(0, 999);
        for (auto& x : d) x = dist(rng);
        datasets.push_back({"Few Unique (1000 values)", std::move(d)});
    }

    // D. Pipe organ (ascending then descending)
    {
        vector<uint32_t> d(N);
        for (size_t i = 0; i < N/2; i++) d[i] = (uint32_t)i;
        for (size_t i = N/2; i < N; i++) d[i] = (uint32_t)(N - i);
        datasets.push_back({"Pipe Organ Pattern", std::move(d)});
    }

    // E. Random 0-65535
    {
        mt19937_64 rng(77);
        vector<uint32_t> d(N);
        uniform_int_distribution<uint32_t> dist(0, 65535);
        for (auto& x : d) x = dist(rng);
        datasets.push_back({"Random 0-65535 (16-bit)", std::move(d)});
    }

    // Run benchmarks
    int wins_qi_scalar = 0, wins_qi_parallel = 0, wins_ska = 0, wins_pdq = 0, wins_std = 0;
    int total = 0;

    for (auto& ds : datasets) {
        cout << "\n--- " << ds.name << " (N=" << N << ") ---\n\n";

        // Reference: std::sort
        auto ref = ds.data;
        double t_std = bench([&]() { auto c = ds.data; sort(c.begin(), c.end()); ref = c; });

        // pdqsort
        double t_pdq = bench([&]() { auto c = ds.data; pdqsort(c.begin(), c.end()); });

        // ska_sort
        double t_ska = bench([&]() { auto c = ds.data; ska_sort(c.begin(), c.end()); });

        // qi::sort scalar
        double t_qi = bench([&]() { auto c = ds.data; qi::sort(c); });

        // qi::sort parallel
        qi::SortOptions pOpts;
        pOpts.parallel = true;
        double t_qip = bench([&]() { auto c = ds.data; qi::sort(c, pOpts); });

        // Correctness
        {
            auto c1 = ds.data; sort(c1.begin(), c1.end());
            auto c2 = ds.data; pdqsort(c2.begin(), c2.end());
            auto c3 = ds.data; ska_sort(c3.begin(), c3.end());
            auto c4 = ds.data; qi::sort(c4);
            auto c5 = ds.data; qi::sort(c5, pOpts);
            if (c2 != c1 || c3 != c1 || c4 != c1 || c5 != c1) {
                cout << "CORRECTNESS FAILURE!\n"; continue;
            }
        }

        // Find winner among single-threaded
        double best_st = min({t_std, t_pdq, t_ska, t_qi});
        double best_all = min({t_std, t_pdq, t_ska, t_qi, t_qip});

        auto tag = [&](double t, bool is_parallel = false) -> string {
            if (!is_parallel && t == best_st) return " <-- FASTEST (single-thread)";
            if (is_parallel && t == best_all && t < best_st) return " <-- FASTEST (overall)";
            return "";
        };

        cout << left << setw(40) << "Algorithm"
             << setw(14) << "Time (ms)"
             << setw(18) << "MKeys/s"
             << "vs std::sort\n";
        cout << string(90, '-') << "\n";

        auto row = [&](const char* name, double t, bool par = false) {
            double mk = N / t / 1000.0;
            string vs = to_string(t_std/t).substr(0,5) + "x";
            cout << setw(40) << name
                 << setw(14) << fixed << setprecision(2) << t
                 << setw(18) << (to_string((int)mk) + " MKeys/s")
                 << vs << tag(t, par) << "\n";
        };

        row("std::sort (C++ Introsort)", t_std);
        row("pdqsort (Rust's std sort)", t_pdq);
        row("ska_sort (Skarupke Radix)", t_ska);
        row("qi::sort SCALAR", t_qi);
        row("qi::sort PARALLEL", t_qip, true);
        cout << string(90, '-') << "\n";

        // Count wins
        total++;
        if (t_qi <= min({t_std, t_pdq, t_ska})) wins_qi_scalar++;
        if (t_qip <= min({t_std, t_pdq, t_ska, t_qi})) wins_qi_parallel++;
        if (t_ska <= min({t_std, t_pdq, t_qi})) wins_ska++;
        if (t_pdq <= min({t_std, t_ska, t_qi})) wins_pdq++;
        if (t_std <= min({t_pdq, t_ska, t_qi})) wins_std++;
    }

    cout << "\n================================================================\n";
    cout << "  SCORECARD (" << total << " datasets)\n";
    cout << "================================================================\n\n";
    cout << "  Single-threaded wins:\n";
    cout << "    std::sort (Introsort)        : " << wins_std << "/" << total << "\n";
    cout << "    pdqsort (Rust's std sort)    : " << wins_pdq << "/" << total << "\n";
    cout << "    ska_sort (Skarupke Radix)    : " << wins_ska << "/" << total << "\n";
    cout << "    qi::sort SCALAR              : " << wins_qi_scalar << "/" << total << "\n";
    cout << "\n  Overall wins (including parallel):\n";
    cout << "    qi::sort PARALLEL            : " << wins_qi_parallel << "/" << total << "\n";
    cout << "================================================================\n\n";

    return 0;
}
