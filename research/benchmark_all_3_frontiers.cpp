#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <string>

// Include All 3 Breakthrough Frontier Engines
#include "../include/qi_apex.hpp"
#include "qi_learned_spline_sort.hpp"
#include "qi_inplace_cyclic_sort.hpp"
#include "qi_simd_vector_sort.hpp"

using namespace std;
using u32 = uint32_t;

template <typename Func>
double time_ms(Func f, int iterations = 5) {
    double best = 1e9;
    for (int i = 0; i < iterations; ++i) {
        auto t0 = chrono::high_resolution_clock::now();
        f();
        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();
        if (ms < best) best = ms;
    }
    return best;
}

void testFrontier(const string& title, const vector<u32>& original) {
    const size_t N = original.size();
    cout << "========================================================================================\n";
    cout << "  ARENA: " << title << " (N = " << N << " elements)\n";
    cout << "========================================================================================\n";
    cout << left << setw(38) << "Engine / Breakthrough Frontier"
         << setw(26) << "Core Innovation"
         << setw(14) << "Time (ms)"
         << setw(20) << "MKeys/s"
         << "Status\n";
    cout << "----------------------------------------------------------------------------------------\n";

    // 0. std::sort
    {
        auto data = original;
        double t = time_ms([&]() { data = original; sort(data.begin(), data.end()); });
        bool ok = is_sorted(data.begin(), data.end());
        cout << left << setw(38) << "std::sort (Baseline)"
             << setw(26) << "Comparison Introsort"
             << setw(14) << fixed << setprecision(2) << t
             << setw(20) << (N / 1e6) / (t / 1000.0)
             << (ok ? "PASS" : "FAIL") << "\n";
    }

    // 1. Frontier 1: Learned Spline Sorter
    {
        auto data = original;
        double t = time_ms([&]() { data = original; qi_learned::sort(data.data(), N); });
        bool ok = is_sorted(data.begin(), data.end());
        cout << left << setw(38) << "[Frontier 1] Learned Spline"
             << setw(26) << "64-Knot CDF + Bitonic"
             << setw(14) << fixed << setprecision(2) << t
             << setw(20) << (N / 1e6) / (t / 1000.0)
             << (ok ? "PASS" : "FAIL") << "\n";
    }

    // 2. Frontier 2: In-Place Cyclic Radix Sorter
    {
        auto data = original;
        double t = time_ms([&]() { data = original; qi_inplace::sort(data.data(), N); });
        bool ok = is_sorted(data.begin(), data.end());
        cout << left << setw(38) << "[Frontier 2] In-Place Cyclic"
             << setw(26) << "O(1) RAM Permutation"
             << setw(14) << fixed << setprecision(2) << t
             << setw(20) << (N / 1e6) / (t / 1000.0)
             << (ok ? "PASS" : "FAIL") << "\n";
    }

    // 3. Frontier 3: Register SIMD Vector Sorter
    {
        auto data = original;
        double t = time_ms([&]() { data = original; qi_simd::sort(data.data(), N); });
        bool ok = is_sorted(data.begin(), data.end());
        cout << left << setw(38) << "[Frontier 3] SIMD Vector Net"
             << setw(26) << "In-Register Bitonic"
             << setw(14) << fixed << setprecision(2) << t
             << setw(20) << (N / 1e6) / (t / 1000.0)
             << (ok ? "PASS" : "FAIL") << "\n";
    }

    // 4. qi::apex ULTIMATE
    {
        auto data = original;
        double t = time_ms([&]() { data = original; qi::apex::sort(data.data(), N); });
        bool ok = is_sorted(data.begin(), data.end());
        cout << left << setw(38) << "qi::apex (Ultimate)"
             << setw(26) << "Strict 20KB L1 Radix"
             << setw(14) << fixed << setprecision(2) << t
             << setw(20) << (N / 1e6) / (t / 1000.0)
             << (ok ? "PASS" : "FAIL") << "\n";
    }

    cout << "----------------------------------------------------------------------------------------\n\n";
}

int main() {
    cout << "========================================================================================\n";
    cout << "  THE 3 UNEXPLORED FRONTIERS SHOWDOWN: EMPIRICAL BREAKTHROUGH EVALUATION\n";
    cout << "  Hardware: Apple Silicon M1 Pro | clang++ -O3 -std=c++17 | Best of 5 Runs\n";
    cout << "========================================================================================\n\n";

    mt19937_64 rng(1337);

    // 100K Elements
    {
        size_t N = 100000;
        vector<u32> data(N);
        for (auto& x : data) x = rng();
        testFrontier("100,000 Uniform Random Keys", data);
    }

    // 1,000,000 Elements
    {
        size_t N = 1000000;
        vector<u32> data(N);
        for (auto& x : data) x = rng();
        testFrontier("1,000,000 Uniform Random Keys", data);
    }

    return 0;
}
