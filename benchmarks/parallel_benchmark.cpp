/*
================================================================
PARALLEL QI-SORT vs THE WORLD
================================================================
Now that qi::sort has parallel mode, let's see where it ranks.
================================================================
*/

#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>
#include <iomanip>
#include <cstring>
#include <thread>
#include "../include/qi_radix.hpp"

using namespace std;
using Clock = chrono::high_resolution_clock;

// Raw non-adaptive parallel radix-16 (baseline competitor)
static void raw_parallel_radix16(uint32_t* data, size_t n, unsigned int numThreads) {
    auto* tmp = new uint32_t[n];

    auto do_pass = [&](uint32_t* src, uint32_t* dst, int shift) {
        vector<vector<size_t>> lc(numThreads, vector<size_t>(65536, 0));
        size_t chunk = n / numThreads;
        vector<thread> threads;

        for (unsigned t = 0; t < numThreads; t++) {
            size_t s = t * chunk, e = (t == numThreads-1) ? n : s + chunk;
            threads.emplace_back([&, s, e, t]() {
                for (size_t i = s; i < e; i++) lc[t][(src[i] >> shift) & 0xFFFF]++;
            });
        }
        for (auto& th : threads) th.join();

        size_t gc[65536] = {};
        for (unsigned t = 0; t < numThreads; t++)
            for (int b = 0; b < 65536; b++) gc[b] += lc[t][b];

        size_t off[65536]; size_t o = 0;
        for (int b = 0; b < 65536; b++) { off[b] = o; o += gc[b]; }

        vector<vector<size_t>> to(numThreads, vector<size_t>(65536, 0));
        for (int b = 0; b < 65536; b++) {
            to[0][b] = off[b];
            for (unsigned t = 1; t < numThreads; t++)
                to[t][b] = to[t-1][b] + lc[t-1][b];
        }

        threads.clear();
        for (unsigned t = 0; t < numThreads; t++) {
            size_t s = t * chunk, e = (t == numThreads-1) ? n : s + chunk;
            threads.emplace_back([&, s, e, t]() {
                for (size_t i = s; i < e; i++)
                    dst[to[t][(src[i] >> shift) & 0xFFFF]++] = src[i];
            });
        }
        for (auto& th : threads) th.join();
    };

    do_pass(data, tmp, 0);
    do_pass(tmp, data, 16);
    delete[] tmp;
}

static inline double ms(function<void()> fn) {
    auto t = Clock::now();
    fn();
    return chrono::duration<double, milli>(Clock::now() - t).count();
}

int main() {
    unsigned int cores = thread::hardware_concurrency();

    cout << "\n================================================================\n";
    cout << "  PARALLEL QI-SORT vs THE WORLD\n";
    cout << "  Cores = " << cores << " | -O3 -march=native -pthread\n";
    cout << "================================================================\n";

    vector<size_t> sizes = {1000000, 5000000, 10000000, 25000000};

    for (size_t N : sizes) {
        cout << "\n--- N = " << N << " (" << (N * 4 / 1024 / 1024) << " MB) ---\n\n";

        mt19937_64 rng(42);
        vector<uint32_t> orig(N);
        uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
        for (auto& x : orig) x = dist(rng);

        // 1. std::sort (single-threaded Introsort)
        auto d1 = orig;
        double t1 = ms([&]() { sort(d1.begin(), d1.end()); });

        // 2. qi::sort SCALAR (single-threaded)
        auto d2 = orig;
        double t2 = ms([&]() { qi::sort(d2); });

        // 3. qi::sort PARALLEL (multi-threaded, all cores)
        auto d3 = orig;
        qi::SortOptions pOpts;
        pOpts.parallel = true;
        double t3 = ms([&]() { qi::sort(d3, pOpts); });

        // 4. Raw parallel radix-16 (no sensing, no adaptation — raw competitor)
        auto d4 = orig;
        double t4 = ms([&]() { raw_parallel_radix16(d4.data(), N, cores); });

        bool ok2 = (d2 == d1), ok3 = (d3 == d1), ok4 = (d4 == d1);

        cout << left
             << setw(45) << "Algorithm"
             << setw(14) << "Time (ms)"
             << setw(18) << "Throughput"
             << setw(18) << "vs std::sort"
             << "OK\n";
        cout << string(100, '-') << "\n";

        auto row = [&](const char* name, double t, bool ok) {
            double mkeys = N / t / 1000.0;
            cout << setw(45) << name
                 << setw(14) << fixed << setprecision(2) << t
                 << setw(18) << (to_string((int)mkeys) + " MKeys/s")
                 << setw(18) << (to_string(t1/t).substr(0,5) + "x FASTER")
                 << (ok ? "PASS" : "FAIL") << "\n";
        };

        row("std::sort (Introsort, 1 thread)", t1, true);
        row("qi::sort SCALAR (1 thread)", t2, ok2);
        row("qi::sort PARALLEL (all cores)", t3, ok3);
        row("Raw Parallel Radix-16 (all cores)", t4, ok4);

        cout << string(100, '-') << "\n";

        // Parallel speedup
        cout << "  Parallel qi::sort vs Scalar qi::sort: "
             << fixed << setprecision(2) << (t2 / t3) << "x speedup\n";
        cout << "  Parallel qi::sort vs Raw Parallel R16: "
             << (t3 < t4 ? to_string(t4/t3).substr(0,4) + "x FASTER" :
                           to_string(t3/t4).substr(0,4) + "x slower") << "\n";
    }

    cout << "\n================================================================\n";
    cout << "  CONCLUSION\n";
    cout << "================================================================\n\n";

    return 0;
}
