#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <cassert>

// Include HyperField v1 & v2
#include "qi_hyper_field_sort.hpp"
#include "qi_hyper_field_v2.hpp"

// Include Apex and Radix
#include "../include/qi_apex.hpp"
#include "../include/qi_radix.hpp"

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

void testN(size_t N, mt19937_64& rng) {
    cout << "========================================================================================\n";
    cout << "  SHOWDOWN: N = " << N << " Elements (Uniform Random 32-bit)\n";
    cout << "========================================================================================\n";
    cout << left << setw(44) << "Algorithm / Engine"
         << setw(16) << "Time (ms)"
         << setw(22) << "Throughput (MKeys/s)"
         << "Speedup vs std::sort\n";
    cout << "----------------------------------------------------------------------------------------\n";

    vector<u32> original(N);
    for (auto& x : original) x = rng();

    // 1. std::sort
    auto d_std = original;
    double t_std = time_ms([&]() { d_std = original; std::sort(d_std.begin(), d_std.end()); });
    assert(is_sorted(d_std.begin(), d_std.end()));
    cout << left << setw(44) << "1. std::sort (Comparison Baseline)"
         << setw(16) << fixed << setprecision(2) << t_std
         << setw(22) << setprecision(1) << (N / 1e6) / (t_std / 1000.0)
         << "1.00x (Baseline)\n";

    // 2. QI-HyperField v1
    auto d_v1 = original;
    double t_v1 = time_ms([&]() { d_v1 = original; qi::hyper_field::sort(d_v1.data(), N); });
    assert(is_sorted(d_v1.begin(), d_v1.end()));
    cout << left << setw(44) << "2. QI-HyperField v1 (Continuous Field)"
         << setw(16) << fixed << setprecision(2) << t_v1
         << setw(22) << setprecision(1) << (N / 1e6) / (t_v1 / 1000.0)
         << setprecision(2) << (t_std / t_v1) << "x FASTER\n";

    // 3. QI-HyperField v2 (Division-Free 1-Cycle Lattice)
    auto d_v2 = original;
    double t_v2 = time_ms([&]() { d_v2 = original; qi::hyper_field_v2::sort(d_v2.data(), N); });
    assert(is_sorted(d_v2.begin(), d_v2.end()));
    cout << left << setw(44) << "3. QI-HyperField v2 (1-Cycle Lattice)"
         << setw(16) << fixed << setprecision(2) << t_v2
         << setw(22) << setprecision(1) << (N / 1e6) / (t_v2 / 1000.0)
         << setprecision(2) << (t_std / t_v2) << "x FASTER\n";

    // 4. qi::sort v0.3.61
    auto d_qi = original;
    double t_qi = time_ms([&]() { d_qi = original; qi::sort(d_qi.data(), N); });
    assert(is_sorted(d_qi.begin(), d_qi.end()));
    cout << left << setw(44) << "4. qi::sort v0.3.61 (Adaptive Radix)"
         << setw(16) << fixed << setprecision(2) << t_qi
         << setw(22) << setprecision(1) << (N / 1e6) / (t_qi / 1000.0)
         << setprecision(2) << (t_std / t_qi) << "x FASTER\n";

    // 5. qi::apex ULTIMATE
    auto d_apex = original;
    double t_apex = time_ms([&]() { d_apex = original; qi::apex::sort(d_apex.data(), N); });
    assert(is_sorted(d_apex.begin(), d_apex.end()));
    cout << left << setw(44) << "5. qi::apex (Strict 20KB L1-Bound)"
         << setw(16) << fixed << setprecision(2) << t_apex
         << setw(22) << setprecision(1) << (N / 1e6) / (t_apex / 1000.0)
         << setprecision(2) << (t_std / t_apex) << "x FASTER\n";

    cout << "----------------------------------------------------------------------------------------\n\n";
}

int main() {
    cout << "========================================================================================\n";
    cout << "  THE HYPERFIELD-V2 SPEED-OF-LIGHT RACE\n";
    cout << "  Hardware: Apple Silicon M1 Pro | clang++ -O3 -std=c++17 | Best of 5 Runs\n";
    cout << "========================================================================================\n\n";

    mt19937_64 rng(1337);

    testN(100000, rng);
    testN(1000000, rng);
    testN(10000000, rng);

    return 0;
}
