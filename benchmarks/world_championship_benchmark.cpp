#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <numeric>
#include <string>
#include <cstring>

// Competitor Headers
#include "competitors/pdqsort.h"
#include "competitors/ska_sort.hpp"

// Our Engines
#include "../include/qi_radix.hpp"
#include "../include/qi_apex.hpp"

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

struct BenchmarkResult {
    string name;
    double timeMs;
    double throughputMKeys;
    bool passed;
};

void runArena(const string& title, const vector<u32>& original, bool testParallel = true) {
    const size_t N = original.size();
    cout << "========================================================================================\n";
    cout << "  ARENA: " << title << " (N = " << N << " elements)\n";
    cout << "========================================================================================\n";
    cout << left << setw(44) << "Algorithm / Engine"
         << setw(16) << "Time (ms)"
         << setw(24) << "Throughput (MKeys/s)"
         << "Status\n";
    cout << "----------------------------------------------------------------------------------------\n";

    vector<BenchmarkResult> results;

    // 1. std::sort
    {
        auto data = original;
        double t = time_ms([&]() { data = original; sort(data.begin(), data.end()); });
        bool ok = is_sorted(data.begin(), data.end());
        double mkeys = (N / 1e6) / (t / 1000.0);
        results.push_back({"std::sort (C++ Introsort)", t, mkeys, ok});
    }

    // 2. pdqsort (Orson Peters)
    {
        auto data = original;
        double t = time_ms([&]() { data = original; pdqsort(data.begin(), data.end()); });
        bool ok = is_sorted(data.begin(), data.end());
        double mkeys = (N / 1e6) / (t / 1000.0);
        results.push_back({"pdqsort (Rust std::sort algorithm)", t, mkeys, ok});
    }

    // 3. ska_sort (Malte Skarupke)
    {
        auto data = original;
        double t = time_ms([&]() { data = original; ska_sort(data.begin(), data.end()); });
        bool ok = is_sorted(data.begin(), data.end());
        double mkeys = (N / 1e6) / (t / 1000.0);
        results.push_back({"ska_sort (American Flag In-Place Radix)", t, mkeys, ok});
    }

    // 4. qi::sort (v0.3.61 Baseline)
    {
        auto data = original;
        double t = time_ms([&]() { data = original; qi::sort(data.data(), N); });
        bool ok = is_sorted(data.begin(), data.end());
        double mkeys = (N / 1e6) / (t / 1000.0);
        results.push_back({"qi::sort (v0.3.61 Baseline)", t, mkeys, ok});
    }

    // 5. qi::apex (Single-Core)
    {
        auto data = original;
        double t = time_ms([&]() { data = original; qi::apex::sort(data.data(), N); });
        bool ok = is_sorted(data.begin(), data.end());
        double mkeys = (N / 1e6) / (t / 1000.0);
        results.push_back({"qi::apex (Single-Core Champion)", t, mkeys, ok});
    }

    if (testParallel && N >= 100000) {
        // 6. qi::apex (Multi-Core Parallel)
        {
            auto data = original;
            double t = time_ms([&]() { data = original; qi::apex::parallel_sort(data.data(), N); });
            bool ok = is_sorted(data.begin(), data.end());
            double mkeys = (N / 1e6) / (t / 1000.0);
            results.push_back({"qi::apex (Multi-Core PARALLEL)", t, mkeys, ok});
        }
    }

    for (const auto& r : results) {
        cout << left << setw(44) << r.name
             << setw(16) << fixed << setprecision(2) << r.timeMs
             << setw(24) << r.throughputMKeys
             << (r.passed ? "PASS" : "FAIL") << "\n";
    }
    cout << "----------------------------------------------------------------------------------------\n\n";
}

int main() {
    cout << "========================================================================================\n";
    cout << "  GLOBAL SORTING WORLD CHAMPIONSHIP: qi::apex vs WORLD'S FASTEST ALGORITHMS\n";
    cout << "  Hardware: Apple Silicon M1 Pro | clang++ -O3 -std=c++17 | Best of 5 Runs\n";
    cout << "========================================================================================\n\n";

    mt19937_64 rng(1337);

    // ── 1. N = 1,000,000 Uniform Random 32-bit ──
    {
        size_t N = 1000000;
        vector<u32> data(N);
        for (size_t i = 0; i < N; ++i) data[i] = rng();
        runArena("1M Uniform Random 32-bit Keys", data);
    }

    // ── 2. N = 10,000,000 Uniform Random 32-bit ──
    {
        size_t N = 10000000;
        vector<u32> data(N);
        for (size_t i = 0; i < N; ++i) data[i] = rng();
        runArena("10M Uniform Random 32-bit Keys", data);
    }

    // ── 3. N = 10,000,000 Narrow Domain (Values 0-255) ──
    {
        size_t N = 10000000;
        vector<u32> data(N);
        uniform_int_distribution<u32> dist(0, 255);
        for (size_t i = 0; i < N; ++i) data[i] = dist(rng);
        runArena("10M Narrow Domain Keys (Values 0-255)", data, false);
    }

    // ── 4. N = 10,000,000 Medium Domain (Values 0-65535) ──
    {
        size_t N = 10000000;
        vector<u32> data(N);
        uniform_int_distribution<u32> dist(0, 65535);
        for (size_t i = 0; i < N; ++i) data[i] = dist(rng);
        runArena("10M Medium Domain Keys (Values 0-65535)", data, false);
    }

    // ── 5. N = 10,000,000 Nearly Sorted (95% Sorted, 5% Swaps) ──
    {
        size_t N = 10000000;
        vector<u32> data(N);
        iota(data.begin(), data.end(), 0);
        for (size_t i = 0; i < N / 20; ++i) {
            size_t a = rng() % N, b = rng() % N;
            swap(data[a], data[b]);
        }
        runArena("10M Nearly Sorted Keys (95% Pre-Sorted)", data, false);
    }

    // ── 6. N = 10,000,000 Clustered Duplicates (1,000 Unique Keys) ──
    {
        size_t N = 10000000;
        vector<u32> data(N);
        uniform_int_distribution<u32> dist(0, 999);
        for (size_t i = 0; i < N; ++i) data[i] = dist(rng);
        runArena("10M Heavy Clustered Duplicates (1000 Unique Values)", data, false);
    }

    // ── 7. N = 10,000,000 Fully Pre-Sorted ──
    {
        size_t N = 10000000;
        vector<u32> data(N);
        for (size_t i = 0; i < N; ++i) data[i] = static_cast<u32>(i);
        runArena("10M Fully Sorted Keys (Monotonic Ascending)", data, false);
    }

    return 0;
}
