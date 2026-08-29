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

// Include Our Flagship Sorters
#include "../include/qi_apex.hpp"
#include "../include/qi_radix.hpp"
#include "../include/qi_hyperfield.hpp"

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

void runStage(const string& stageName, vector<u32>& original) {
    const size_t N = original.size();
    cout << "========================================================================================\n";
    cout << "  STAGE: " << stageName << " (N = " << N << " Keys, 40 MB RAM)\n";
    cout << "========================================================================================\n";
    cout << left << setw(40) << "Algorithm / Engine"
         << setw(14) << "Time (ms)"
         << setw(20) << "Throughput (MKeys/s)"
         << setw(16) << "vs std::sort"
         << "vs qi::apex\n";
    cout << "----------------------------------------------------------------------------------------\n";

    // 1. std::sort
    auto d_std = original;
    double t_std = time_ms([&]() { d_std = original; std::sort(d_std.begin(), d_std.end()); });
    assert(is_sorted(d_std.begin(), d_std.end()));

    // 2. pdqsort
    auto d_pdq = original;
    double t_pdq = time_ms([&]() { d_pdq = original; pdqsort(d_pdq.begin(), d_pdq.end()); });
    assert(is_sorted(d_pdq.begin(), d_pdq.end()));

    // 3. qi::sort
    auto d_qi = original;
    double t_qi = time_ms([&]() { d_qi = original; qi::sort(d_qi.data(), N); });
    assert(is_sorted(d_qi.begin(), d_qi.end()));

    // 4. qi::hyperfield
    auto d_hf = original;
    double t_hf = time_ms([&]() { d_hf = original; qi::hyperfield::sort(d_hf.data(), N); });
    assert(is_sorted(d_hf.begin(), d_hf.end()));

    // 5. qi::apex (Single-Core)
    auto d_apex = original;
    double t_apex = time_ms([&]() { d_apex = original; qi::apex::sort(d_apex.data(), N); });
    assert(is_sorted(d_apex.begin(), d_apex.end()));

    // 6. qi::apex (Multi-Core Parallel)
    auto d_par = original;
    double t_par = time_ms([&]() { d_par = original; qi::apex::parallel_sort(d_par.data(), N); });
    assert(is_sorted(d_par.begin(), d_par.end()));

    // Print rows
    auto printRow = [&](const string& name, double t) {
        double mkeys = (N / 1e6) / (t / 1000.0);
        double speedup_std = t_std / t;
        double speedup_apex = t_apex / t;
        cout << left << setw(40) << name
             << setw(14) << fixed << setprecision(2) << t
             << setw(20) << setprecision(1) << mkeys
             << setw(16) << (to_string((int)speedup_std) + "." + to_string((int)(speedup_std*100)%100) + "x")
             << (to_string((int)speedup_apex) + "." + to_string((int)(speedup_apex*100)%100) + "x") << "\n";
    };

    printRow("1. std::sort (C++ Baseline)", t_std);
    printRow("2. boost::sort::pdqsort", t_pdq);
    printRow("3. qi::sort v0.3.61 (Adaptive)", t_qi);
    printRow("4. qi::hyperfield (Continuous Field)", t_hf);
    printRow("5. qi::apex (Single-Core Champion)", t_apex);
    printRow("6. qi::apex (Multi-Core Parallel)", t_par);

    cout << "----------------------------------------------------------------------------------------\n\n";
}

int main() {
    cout << "========================================================================================\n";
    cout << "  THE ULTIMATE HARD BENCHMARK ARENA: APEX vs HYPERFIELD vs QI::SORT vs PDQSORT\n";
    cout << "  Workload Scale: 10,000,000 Keys per Test (40 MB RAM) | Best of 3 Runs\n";
    cout << "  Hardware: Apple Silicon M1 Pro | clang++ -O3 -std=c++17\n";
    cout << "========================================================================================\n\n";

    mt19937_64 rng(1337);
    const size_t N = 10000000;

    // 1. Uniform Random 32-bit (Full Entropy)
    {
        vector<u32> data(N);
        for (auto& x : data) x = rng();
        runStage("1. Uniform Random 32-bit (Full Entropy)", data);
    }

    // 2. Extreme Low Cardinality (4 Categories: 0, 1, 2, 3)
    {
        vector<u32> data(N);
        for (auto& x : data) x = rng() % 4;
        runStage("2. 4-Category Low Cardinality (0, 1, 2, 3)", data);
    }

    // 3. Byte Duplicates (0 to 255)
    {
        vector<u32> data(N);
        for (auto& x : data) x = rng() % 256;
        runStage("3. Byte Duplicates (0 to 255 Categories)", data);
    }

    // 4. 16-bit Domain Range (0 to 65,535)
    {
        vector<u32> data(N);
        for (auto& x : data) x = rng() & 0xFFFFu;
        runStage("4. 16-bit Bounded Domain (0 to 65,535)", data);
    }

    // 5. Pre-Sorted Monotonic
    {
        vector<u32> data(N);
        for (size_t i = 0; i < N; ++i) data[i] = i;
        runStage("5. Already Sorted Monotonic Ascending", data);
    }

    // 6. Reverse Sorted Monotonic
    {
        vector<u32> data(N);
        for (size_t i = 0; i < N; ++i) data[i] = N - 1 - i;
        runStage("6. Reverse Sorted Monotonic Descending", data);
    }

    // 7. Nearly Sorted Monotonic (~98% sorted)
    {
        vector<u32> data(N);
        for (size_t i = 0; i < N; ++i) data[i] = i;
        for (size_t i = 0; i < N / 50; ++i) {
            size_t idx = rng() % N;
            data[idx] = rng() % N;
        }
        runStage("7. Nearly Sorted Monotonic (~98% In-Order)", data);
    }

    // 8. Clustered Gaussian Normal Distribution
    {
        vector<u32> data(N);
        normal_distribution<double> norm(2000000000.0, 500000.0);
        for (auto& x : data) x = static_cast<u32>(norm(rng));
        runStage("8. Clustered Gaussian Distribution", data);
    }

    // 9. Sawtooth Ramps (Period = 1000)
    {
        vector<u32> data(N);
        for (size_t i = 0; i < N; ++i) data[i] = i % 1000;
        runStage("9. Sawtooth Ramps (Period = 1,000)", data);
    }

    // 10. Pipe Organ Distribution (Ascending then Descending)
    {
        vector<u32> data(N);
        size_t mid = N / 2;
        for (size_t i = 0; i < mid; ++i) data[i] = i;
        for (size_t i = mid; i < N; ++i) data[i] = N - 1 - i;
        runStage("10. Pipe Organ (Ascending then Descending)", data);
    }

    return 0;
}
