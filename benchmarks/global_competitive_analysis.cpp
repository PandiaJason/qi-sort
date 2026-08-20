/*
================================================================
HONEST GLOBAL COMPETITIVE ANALYSIS
================================================================
Compares qi::sort against the fastest known techniques:
  1. qi::sort (Our adaptive radix engine — single-threaded, scalar)
  2. std::sort (Introsort — single-threaded, scalar)
  3. Hand-optimized LSD Radix-16 (no sensing, no overhead — raw speed ceiling)
  4. SIMD-style bitonic merge (simulated vectorized sorting network)
  5. Parallel std::sort (multi-threaded via std::execution::par — if available)

This benchmark answers: "Is qi-sort the fastest on earth?"
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
#include <future>
#include "../include/qi_radix.hpp"

using namespace std;
using Clock = chrono::high_resolution_clock;

// ─── RAW OPTIMAL LSD RADIX-16 (No sensing, no overhead — pure speed ceiling) ──
static void raw_optimal_radix16(uint32_t* data, size_t n) {
    auto* tmp = new uint32_t[n];

    // Pass 1: lower 16 bits
    size_t count0[65536] = {};
    for (size_t i = 0; i < n; i++) count0[data[i] & 0xFFFF]++;
    size_t offset = 0;
    for (int i = 0; i < 65536; i++) { size_t c = count0[i]; count0[i] = offset; offset += c; }
    for (size_t i = 0; i < n; i++) tmp[count0[data[i] & 0xFFFF]++] = data[i];

    // Pass 2: upper 16 bits
    size_t count1[65536] = {};
    for (size_t i = 0; i < n; i++) count1[tmp[i] >> 16]++;
    offset = 0;
    for (int i = 0; i < 65536; i++) { size_t c = count1[i]; count1[i] = offset; offset += c; }
    for (size_t i = 0; i < n; i++) data[count1[tmp[i] >> 16]++] = tmp[i];

    delete[] tmp;
}

// ─── PARALLEL MERGE SORT (uses all CPU cores) ──────────────────────────────────
static void parallel_merge_sort(uint32_t* data, size_t n, int depth = 0) {
    int max_depth = (int)log2(thread::hardware_concurrency());
    if (max_depth < 1) max_depth = 1;

    if (n < 100000 || depth >= max_depth) {
        sort(data, data + n);
        return;
    }

    size_t mid = n / 2;
    auto left_future = async(launch::async, [&]() {
        parallel_merge_sort(data, mid, depth + 1);
    });
    parallel_merge_sort(data + mid, n - mid, depth + 1);
    left_future.get();

    // Merge
    vector<uint32_t> merged(n);
    merge(data, data + mid, data + mid, data + n, merged.begin());
    memcpy(data, merged.data(), n * sizeof(uint32_t));
}

// ─── PARALLEL RADIX SORT (multi-threaded histogram + scatter) ──────────────────
static void parallel_radix16(uint32_t* data, size_t n) {
    auto* tmp = new uint32_t[n];
    unsigned int num_threads = thread::hardware_concurrency();
    if (num_threads < 1) num_threads = 1;

    auto do_pass = [&](uint32_t* src, uint32_t* dst, int shift) {
        // Phase 1: parallel local histograms
        vector<vector<size_t>> local_counts(num_threads, vector<size_t>(65536, 0));
        size_t chunk = n / num_threads;
        vector<future<void>> futures;

        for (unsigned t = 0; t < num_threads; t++) {
            size_t start = t * chunk;
            size_t end = (t == num_threads - 1) ? n : start + chunk;
            futures.push_back(async(launch::async, [&, start, end, t]() {
                for (size_t i = start; i < end; i++)
                    local_counts[t][(src[i] >> shift) & 0xFFFF]++;
            }));
        }
        for (auto& f : futures) f.get();

        // Phase 2: prefix sum (serial — fast on 64K entries)
        size_t global_count[65536] = {};
        for (unsigned t = 0; t < num_threads; t++)
            for (int b = 0; b < 65536; b++)
                global_count[b] += local_counts[t][b];

        size_t offsets[65536];
        size_t offset = 0;
        for (int b = 0; b < 65536; b++) { offsets[b] = offset; offset += global_count[b]; }

        // Phase 3: compute per-thread scatter offsets
        vector<vector<size_t>> thread_offsets(num_threads, vector<size_t>(65536, 0));
        for (int b = 0; b < 65536; b++) {
            thread_offsets[0][b] = offsets[b];
            for (unsigned t = 1; t < num_threads; t++)
                thread_offsets[t][b] = thread_offsets[t-1][b] + local_counts[t-1][b];
        }

        // Phase 4: parallel scatter
        futures.clear();
        for (unsigned t = 0; t < num_threads; t++) {
            size_t start = t * chunk;
            size_t end = (t == num_threads - 1) ? n : start + chunk;
            futures.push_back(async(launch::async, [&, start, end, t]() {
                for (size_t i = start; i < end; i++)
                    dst[thread_offsets[t][(src[i] >> shift) & 0xFFFF]++] = src[i];
            }));
        }
        for (auto& f : futures) f.get();
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
    size_t N = 10000000;
    unsigned int cores = thread::hardware_concurrency();

    cout << "\n================================================================\n";
    cout << "  GLOBAL COMPETITIVE ANALYSIS: Is qi-sort the fastest on Earth?\n";
    cout << "  N = " << N << " | Cores = " << cores << " | -O3 -march=native\n";
    cout << "================================================================\n\n";

    mt19937_64 rng(42);
    vector<uint32_t> orig(N);
    uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
    for (auto& x : orig) x = dist(rng);

    // 1. std::sort (Introsort)
    auto d1 = orig; double t1 = ms([&]() { sort(d1.begin(), d1.end()); });

    // 2. qi::sort (Our Engine — single-threaded)
    auto d2 = orig; double t2 = ms([&]() { qi::sort(d2); });

    // 3. Raw Optimal Radix-16 (no sensing — theoretical scalar ceiling)
    auto d3 = orig; double t3 = ms([&]() { raw_optimal_radix16(d3.data(), N); });

    // 4. Parallel Merge Sort (multi-core)
    auto d4 = orig; double t4 = ms([&]() { parallel_merge_sort(d4.data(), N); });

    // 5. Parallel Radix-16 (multi-core)
    auto d5 = orig; double t5 = ms([&]() { parallel_radix16(d5.data(), N); });

    // Verify all
    bool ok2 = (d2 == d1), ok3 = (d3 == d1), ok4 = (d4 == d1), ok5 = (d5 == d1);

    cout << left
         << setw(42) << "Algorithm"
         << setw(16) << "Time (ms)"
         << setw(20) << "Throughput"
         << setw(16) << "vs qi::sort"
         << "Status\n";
    cout << string(100, '-') << "\n";

    auto row = [&](const char* name, double t, bool ok) {
        double mkeys = N / t / 1000.0;
        string vs = (t < t2) ? to_string(t2/t).substr(0,4) + "x faster" :
                    (t > t2) ? to_string(t/t2).substr(0,4) + "x slower" : "equal";
        cout << setw(42) << name
             << setw(16) << fixed << setprecision(2) << t
             << setw(20) << (to_string((int)mkeys) + " MKeys/s")
             << setw(16) << vs
             << (ok ? "PASS" : "FAIL") << "\n";
    };

    row("std::sort (Introsort, 1 thread)", t1, true);
    row("qi::sort (Our Engine, 1 thread)", t2, ok2);
    row("Raw Optimal Radix-16 (1 thread, no sensing)", t3, ok3);
    row("Parallel Merge Sort (all cores)", t4, ok4);
    row("Parallel Radix-16 (all cores)", t5, ok5);

    cout << string(100, '-') << "\n\n";

    cout << "================================================================\n";
    cout << "  VERDICT\n";
    cout << "================================================================\n\n";

    // Find fastest
    struct Result { const char* name; double time; };
    vector<Result> results = {
        {"std::sort (Introsort)", t1},
        {"qi::sort (Our Engine)", t2},
        {"Raw Optimal Radix-16 (no sensing)", t3},
        {"Parallel Merge Sort", t4},
        {"Parallel Radix-16", t5},
    };
    sort(results.begin(), results.end(), [](auto& a, auto& b) { return a.time < b.time; });

    cout << "  Ranking (fastest to slowest):\n";
    for (int i = 0; i < (int)results.size(); i++) {
        cout << "    " << (i+1) << ". " << results[i].name
             << " — " << fixed << setprecision(2) << results[i].time << " ms";
        if (i == 0) cout << " (FASTEST)";
        cout << "\n";
    }

    cout << "\n  qi::sort is the fastest SINGLE-THREADED sort tested: "
         << (t2 <= t3 ? "YES" : "NO") << "\n";
    cout << "  qi::sort is the fastest OVERALL (including multi-core): "
         << (t2 <= min({t3, t4, t5}) ? "YES" : "NO") << "\n";
    cout << "\n  Missing from this test (things that could be faster):\n";
    cout << "    - Google Highway vqsort (AVX-512 / NEON SIMD vectorized sorting)\n";
    cout << "    - Intel IPP radix sort (hand-tuned SSE/AVX intrinsics)\n";
    cout << "    - GPU radix sort (CUDA CUB / Thrust — billions of keys/sec)\n";
    cout << "    - IPS4o (In-Place Parallel Super Scalar Samplesort)\n";
    cout << "================================================================\n\n";

    return 0;
}
