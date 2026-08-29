#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <cassert>

// Include Boost sorters
#include "../third_party/boost_real/boost_sort_real.hpp"

// Include Flagship Engines
#include "../include/qi_apex.hpp"
#include "../include/qi_radix.hpp"

using namespace std;
using u32 = uint32_t;

template <typename Func>
double time_ms(Func f, int iterations = 3) {
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

void runBenchmarkRow(const string& testName, vector<u32>& original) {
    const size_t N = original.size();
    
    // Single-Threaded Competitors (1T)
    auto d_std = original;
    double t_std = time_ms([&]() { d_std = original; std::sort(d_std.begin(), d_std.end()); });
    assert(is_sorted(d_std.begin(), d_std.end()));

    auto d_pdq = original;
    double t_pdq = time_ms([&]() { d_pdq = original; pdqsort(d_pdq.begin(), d_pdq.end()); });
    assert(is_sorted(d_pdq.begin(), d_pdq.end()));

    auto d_spread = original;
    double t_spread = time_ms([&]() { d_spread = original; boost_compat::spreadsort(d_spread.begin(), d_spread.end()); });
    assert(is_sorted(d_spread.begin(), d_spread.end()));

    auto d_qi = original;
    double t_qi = time_ms([&]() { d_qi = original; qi::sort(d_qi.data(), N); });
    assert(is_sorted(d_qi.begin(), d_qi.end()));

    auto d_apex = original;
    double t_apex = time_ms([&]() { d_apex = original; qi::apex::sort(d_apex.data(), N); });
    assert(is_sorted(d_apex.begin(), d_apex.end()));

    // Multi-Threaded Competitor (MT)
    auto d_par = original;
    double t_par = time_ms([&]() { d_par = original; qi::apex::parallel_sort(d_par.data(), N); });
    assert(is_sorted(d_par.begin(), d_par.end()));

    // 1T Winner among tested sorters
    double best_1t = min({t_std, t_pdq, t_spread, t_qi, t_apex});
    string winner_1t = "";
    if (best_1t == t_apex) winner_1t = "qi::apex (1T)";
    else if (best_1t == t_qi) winner_1t = "qi::sort (1T)";
    else if (best_1t == t_pdq) winner_1t = "pdqsort (1T)";
    else if (best_1t == t_std) winner_1t = "std::sort (1T)";
    else winner_1t = "spreadsort (1T)";

    cout << left << setw(30) << testName
         << setw(11) << fixed << setprecision(2) << t_std
         << setw(11) << t_pdq
         << setw(11) << t_spread
         << setw(11) << t_qi
         << setw(11) << t_apex
         << setw(16) << winner_1t
         << setw(12) << t_par << "\n";
}

int main() {
    cout << "========================================================================================================================\n";
    cout << "  MASTER GLOBAL SORTING BENCHMARK SCORECARD\n";
    cout << "  Comparing: std::sort vs pdqsort vs spreadsort vs qi::sort vs qi::apex\n";
    cout << "  Hardware: Apple Silicon M1 Pro | clang++ -O3 -std=c++17 | Best of 3 Runs\n";
    cout << "========================================================================================================================\n\n";

    mt19937_64 rng(42);

    // ════════════════════════════════════════════════════════════════════════════
    // SECTION 1: N = 1,000,000 KEYS (4 MB RAM)
    // ════════════════════════════════════════════════════════════════════════════
    cout << "========================================================================================================================\n";
    cout << "  SCALE: N = 1,000,000 KEYS (4 MB RAM)\n";
    cout << "========================================================================================================================\n";
    cout << left << setw(30) << "Workload Distribution"
         << setw(11) << "std::sort"
         << setw(11) << "pdqsort"
         << setw(11) << "spread"
         << setw(11) << "qi::sort"
         << setw(11) << "qi::apex"
         << setw(16) << "Winner (1T)"
         << setw(12) << "apex (Par MT)\n";
    cout << "------------------------------------------------------------------------------------------------------------------------\n";

    const size_t N1M = 1000000;
    {
        vector<u32> data(N1M); for (auto& x : data) x = rng();
        runBenchmarkRow("1. Uniform Random 32-bit", data);
    }
    {
        vector<u32> data(N1M); for (auto& x : data) x = rng() % 4;
        runBenchmarkRow("2. Low-Card (4 values)", data);
    }
    {
        vector<u32> data(N1M); for (auto& x : data) x = rng() % 256;
        runBenchmarkRow("3. Byte Dups (0-255)", data);
    }
    {
        vector<u32> data(N1M); for (auto& x : data) x = rng() & 0xFFFFu;
        runBenchmarkRow("4. 16-bit Domain (0-65K)", data);
    }
    {
        vector<u32> data(N1M); for (size_t i = 0; i < N1M; ++i) data[i] = i;
        runBenchmarkRow("5. Already Sorted", data);
    }
    {
        vector<u32> data(N1M); for (size_t i = 0; i < N1M; ++i) data[i] = N1M - 1 - i;
        runBenchmarkRow("6. Reverse Sorted", data);
    }
    {
        vector<u32> data(N1M); for (size_t i = 0; i < N1M; ++i) data[i] = i;
        for (size_t i = 0; i < N1M / 50; ++i) data[rng() % N1M] = rng() % N1M;
        runBenchmarkRow("7. Nearly Sorted (~98%)", data);
    }
    {
        vector<u32> data(N1M); for (size_t i = 0; i < N1M; ++i) data[i] = i % 1000;
        runBenchmarkRow("8. Sawtooth (Period 1K)", data);
    }
    {
        vector<u32> data(N1M);
        size_t mid = N1M / 2;
        for (size_t i = 0; i < mid; ++i) data[i] = i;
        for (size_t i = mid; i < N1M; ++i) data[i] = N1M - 1 - i;
        runBenchmarkRow("9. Pipe Organ", data);
    }

    cout << "------------------------------------------------------------------------------------------------------------------------\n\n";

    // ════════════════════════════════════════════════════════════════════════════
    // SECTION 2: N = 10,000,000 KEYS (40 MB RAM)
    // ════════════════════════════════════════════════════════════════════════════
    cout << "========================================================================================================================\n";
    cout << "  SCALE: N = 10,000,000 KEYS (40 MB RAM)\n";
    cout << "========================================================================================================================\n";
    cout << left << setw(30) << "Workload Distribution"
         << setw(11) << "std::sort"
         << setw(11) << "pdqsort"
         << setw(11) << "spread"
         << setw(11) << "qi::sort"
         << setw(11) << "qi::apex"
         << setw(16) << "Winner (1T)"
         << setw(12) << "apex (Par MT)\n";
    cout << "------------------------------------------------------------------------------------------------------------------------\n";

    const size_t N10M = 10000000;
    {
        vector<u32> data(N10M); for (auto& x : data) x = rng();
        runBenchmarkRow("1. Uniform Random 32-bit", data);
    }
    {
        vector<u32> data(N10M); for (auto& x : data) x = rng() % 4;
        runBenchmarkRow("2. Low-Card (4 values)", data);
    }
    {
        vector<u32> data(N10M); for (auto& x : data) x = rng() % 256;
        runBenchmarkRow("3. Byte Dups (0-255)", data);
    }
    {
        vector<u32> data(N10M); for (auto& x : data) x = rng() & 0xFFFFu;
        runBenchmarkRow("4. 16-bit Domain (0-65K)", data);
    }
    {
        vector<u32> data(N10M); for (size_t i = 0; i < N10M; ++i) data[i] = i;
        runBenchmarkRow("5. Already Sorted", data);
    }
    {
        vector<u32> data(N10M); for (size_t i = 0; i < N10M; ++i) data[i] = N10M - 1 - i;
        runBenchmarkRow("6. Reverse Sorted", data);
    }
    {
        vector<u32> data(N10M); for (size_t i = 0; i < N10M; ++i) data[i] = i;
        for (size_t i = 0; i < N10M / 50; ++i) data[rng() % N10M] = rng() % N10M;
        runBenchmarkRow("7. Nearly Sorted (~98%)", data);
    }
    {
        vector<u32> data(N10M); for (size_t i = 0; i < N10M; ++i) data[i] = i % 1000;
        runBenchmarkRow("8. Sawtooth (Period 1K)", data);
    }
    {
        vector<u32> data(N10M);
        size_t mid = N10M / 2;
        for (size_t i = 0; i < mid; ++i) data[i] = i;
        for (size_t i = mid; i < N10M; ++i) data[i] = N10M - 1 - i;
        runBenchmarkRow("9. Pipe Organ", data);
    }

    cout << "------------------------------------------------------------------------------------------------------------------------\n";

    return 0;
}
