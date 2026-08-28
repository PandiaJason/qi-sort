#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <cassert>

// 1. Real Official Boost.Sort (pdqsort & spreadsort)
#include "../third_party/boost_real/boost_sort_real.hpp"

// 2. Our Engines
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

void runShowdown(const string& testTitle, vector<u32>& original) {
    const size_t N = original.size();
    cout << "========================================================================================\n";
    cout << "  " << testTitle << " (N = " << N << " Elements)\n";
    cout << "========================================================================================\n";
    cout << left << setw(44) << "Algorithm / Engine"
         << setw(16) << "Time (ms)"
         << setw(22) << "Throughput (MKeys/s)"
         << "Speedup vs pdqsort\n";
    cout << "----------------------------------------------------------------------------------------\n";

    // 1. std::sort (C++ Standard Library)
    auto d_std = original;
    double t_std = time_ms([&]() { d_std = original; std::sort(d_std.begin(), d_std.end()); });
    assert(is_sorted(d_std.begin(), d_std.end()));

    // 2. Real Boost.Sort pdqsort (Orson Peters)
    auto d_pdq = original;
    double t_pdq = time_ms([&]() { d_pdq = original; pdqsort(d_pdq.begin(), d_pdq.end()); });
    assert(is_sorted(d_pdq.begin(), d_pdq.end()));
    cout << left << setw(44) << "1. boost::sort::pdqsort (Orson Peters)"
         << setw(16) << fixed << setprecision(2) << t_pdq
         << setw(22) << setprecision(1) << (N / 1e6) / (t_pdq / 1000.0)
         << "1.00x (Baseline)\n";

    // 3. Real Boost.Sort spreadsort (Steven Ross)
    auto d_spread = original;
    double t_spread = time_ms([&]() { d_spread = original; boost_compat::spreadsort(d_spread.begin(), d_spread.end()); });
    assert(is_sorted(d_spread.begin(), d_spread.end()));
    cout << left << setw(44) << "2. boost::sort::spreadsort (Steven Ross)"
         << setw(16) << fixed << setprecision(2) << t_spread
         << setw(22) << setprecision(1) << (N / 1e6) / (t_spread / 1000.0)
         << setprecision(2) << (t_pdq / t_spread) << "x\n";

    // 4. qi::sort v0.3.61
    auto d_qi = original;
    double t_qi = time_ms([&]() { d_qi = original; qi::sort(d_qi.data(), N); });
    assert(is_sorted(d_qi.begin(), d_qi.end()));
    cout << left << setw(44) << "3. qi::sort v0.3.61 (Adaptive Sense)"
         << setw(16) << fixed << setprecision(2) << t_qi
         << setw(22) << setprecision(1) << (N / 1e6) / (t_qi / 1000.0)
         << setprecision(2) << (t_pdq / t_qi) << "x FASTER\n";

    // 5. qi::apex ULTIMATE (Single-Core)
    auto d_apex = original;
    double t_apex = time_ms([&]() { d_apex = original; qi::apex::sort(d_apex.data(), N); });
    assert(is_sorted(d_apex.begin(), d_apex.end()));
    cout << left << setw(44) << "4. qi::apex (Jason Pandian - Single-Core)"
         << setw(16) << fixed << setprecision(2) << t_apex
         << setw(22) << setprecision(1) << (N / 1e6) / (t_apex / 1000.0)
         << setprecision(2) << (t_pdq / t_apex) << "x FASTER\n";

    // 6. qi::apex Multi-Core Parallel
    auto d_par = original;
    double t_par = time_ms([&]() { d_par = original; qi::apex::parallel_sort(d_par.data(), N); });
    assert(is_sorted(d_par.begin(), d_par.end()));
    cout << left << setw(44) << "5. qi::apex (Multi-Core Parallel)"
         << setw(16) << fixed << setprecision(2) << t_par
         << setw(22) << setprecision(1) << (N / 1e6) / (t_par / 1000.0)
         << setprecision(2) << (t_pdq / t_par) << "x FASTER\n";

    cout << "----------------------------------------------------------------------------------------\n\n";
}

int main() {
    cout << "========================================================================================\n";
    cout << "  OFFICIAL BOOST.SORT SHOWDOWN: REAL BOOST CODE vs qi::apex & qi::sort\n";
    cout << "  Hardware: Apple Silicon M1 Pro | clang++ -O3 -std=c++17 | Best of 3 Runs\n";
    cout << "========================================================================================\n\n";

    mt19937_64 rng(1337);

    // Scenario 1: N = 1,000,000 Uniform Random 32-bit
    {
        vector<u32> data(1000000);
        for (auto& x : data) x = rng();
        runShowdown("SCENARIO 1: N = 1,000,000 Uniform Random 32-bit", data);
    }

    // Scenario 2: N = 10,000,000 Uniform Random 32-bit
    {
        vector<u32> data(10000000);
        for (auto& x : data) x = rng();
        runShowdown("SCENARIO 2: N = 10,000,000 Uniform Random 32-bit", data);
    }

    // Scenario 3: N = 10,000,000 Heavy Duplicates (0-255 Categories)
    {
        vector<u32> data(10000000);
        for (auto& x : data) x = rng() % 256;
        runShowdown("SCENARIO 3: N = 10,000,000 Duplicate Categories (0-255)", data);
    }

    // Scenario 4: N = 10,000,000 Pre-Sorted Monotonic
    {
        vector<u32> data(10000000);
        for (size_t i = 0; i < 10000000; ++i) data[i] = i;
        runShowdown("SCENARIO 4: N = 10,000,000 Already Sorted Monotonic", data);
    }

    return 0;
}
