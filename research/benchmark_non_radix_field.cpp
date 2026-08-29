#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <cassert>

// Include non-radix continuous field and vector sorters
#include "qi_field_sort.hpp"
#include "qi_wave_sort.hpp"
#include "qi_hyper_field_sort.hpp"
#include "qi_simd_vector_sort.hpp"

// Include radix sorters
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

void runArena(const string& title, vector<u32>& original) {
    const size_t N = original.size();
    cout << "========================================================================================\n";
    cout << "  " << title << " (N = " << N << " Elements)\n";
    cout << "========================================================================================\n";
    cout << left << setw(44) << "Algorithm / Paradigm"
         << setw(16) << "Time (ms)"
         << setw(22) << "Throughput (MKeys/s)"
         << "Speedup vs std::sort\n";
    cout << "----------------------------------------------------------------------------------------\n";

    // 1. std::sort
    auto d_std = original;
    double t_std = time_ms([&]() { d_std = original; std::sort(d_std.begin(), d_std.end()); });
    assert(is_sorted(d_std.begin(), d_std.end()));
    cout << left << setw(44) << "1. std::sort (Comparison Baseline)"
         << setw(16) << fixed << setprecision(2) << t_std
         << setw(22) << setprecision(1) << (N / 1e6) / (t_std / 1000.0)
         << "1.00x (Baseline)\n";

    // 2. QI-FieldSort v1 (100% Non-Radix Density Inversion)
    auto d_f1 = original;
    double t_f1 = time_ms([&]() { d_f1 = original; qi_field::sort(d_f1.data(), N); });
    assert(is_sorted(d_f1.begin(), d_f1.end()));
    cout << left << setw(44) << "2. QI-FieldSort v1 (Non-Radix Continuous)"
         << setw(16) << fixed << setprecision(2) << t_f1
         << setw(22) << setprecision(1) << (N / 1e6) / (t_f1 / 1000.0)
         << setprecision(2) << (t_std / t_f1) << "x FASTER\n";

    // 3. QI-WaveSort (Wavefunction Collapse + Block Cache)
    auto d_wave = original;
    double t_wave = time_ms([&]() { d_wave = original; qi_wave::sort(d_wave.data(), N); });
    assert(is_sorted(d_wave.begin(), d_wave.end()));
    cout << left << setw(44) << "3. QI-WaveSort (Wavefunction Collapse)"
         << setw(16) << fixed << setprecision(2) << t_wave
         << setw(22) << setprecision(1) << (N / 1e6) / (t_wave / 1000.0)
         << setprecision(2) << (t_std / t_wave) << "x FASTER\n";

    // 4. QI-HyperFieldSort (1-Pass Direct Density Projection)
    auto d_hf = original;
    double t_hf = time_ms([&]() { d_hf = original; qi::hyper_field::sort(d_hf.data(), N); });
    assert(is_sorted(d_hf.begin(), d_hf.end()));
    cout << left << setw(44) << "4. QI-HyperFieldSort (1-Pass Density)"
         << setw(16) << fixed << setprecision(2) << t_hf
         << setw(22) << setprecision(1) << (N / 1e6) / (t_hf / 1000.0)
         << setprecision(2) << (t_std / t_hf) << "x FASTER\n";

    // 5. QI-SIMDVectorSort (In-Register Bitonic Vector Sorter)
    auto d_simd = original;
    double t_simd = time_ms([&]() { d_simd = original; qi_simd::sort(d_simd.data(), N); });
    assert(is_sorted(d_simd.begin(), d_simd.end()));
    cout << left << setw(44) << "5. QI-SIMDVectorSort (In-Register Vector)"
         << setw(16) << fixed << setprecision(2) << t_simd
         << setw(22) << setprecision(1) << (N / 1e6) / (t_simd / 1000.0)
         << setprecision(2) << (t_std / t_simd) << "x FASTER\n";

    // 6. qi::sort v0.3.61
    auto d_qi = original;
    double t_qi = time_ms([&]() { d_qi = original; qi::sort(d_qi.data(), N); });
    assert(is_sorted(d_qi.begin(), d_qi.end()));
    cout << left << setw(44) << "6. qi::sort v0.3.61 (Adaptive Radix)"
         << setw(16) << fixed << setprecision(2) << t_qi
         << setw(22) << setprecision(1) << (N / 1e6) / (t_qi / 1000.0)
         << setprecision(2) << (t_std / t_qi) << "x FASTER\n";

    // 7. qi::apex ULTIMATE
    auto d_apex = original;
    double t_apex = time_ms([&]() { d_apex = original; qi::apex::sort(d_apex.data(), N); });
    assert(is_sorted(d_apex.begin(), d_apex.end()));
    cout << left << setw(44) << "7. qi::apex (Strict 20KB L1-Bound)"
         << setw(16) << fixed << setprecision(2) << t_apex
         << setw(22) << setprecision(1) << (N / 1e6) / (t_apex / 1000.0)
         << setprecision(2) << (t_std / t_apex) << "x FASTER\n";

    cout << "----------------------------------------------------------------------------------------\n\n";
}

int main() {
    cout << "========================================================================================\n";
    cout << "  THE NON-RADIX CONTINUOUS FIELD SHOWDOWN: FIELD/WAVE vs APEX vs STD::SORT\n";
    cout << "  Hardware: Apple Silicon M1 Pro | clang++ -O3 -std=c++17 | Best of 3 Runs\n";
    cout << "========================================================================================\n\n";

    mt19937_64 rng(42);

    // Test 1: N = 100,000 Uniform Random 32-bit
    {
        vector<u32> data(100000);
        for (auto& x : data) x = rng();
        runArena("TEST 1: N = 100,000 Uniform Random 32-bit", data);
    }

    // Test 2: N = 1,000,000 Uniform Random 32-bit
    {
        vector<u32> data(1000000);
        for (auto& x : data) x = rng();
        runArena("TEST 2: N = 1,000,000 Uniform Random 32-bit", data);
    }

    // Test 3: N = 1,000,000 Clustered Gaussian Distribution
    {
        vector<u32> data(1000000);
        normal_distribution<double> norm(2000000000.0, 500000.0);
        for (auto& x : data) x = static_cast<u32>(norm(rng));
        runArena("TEST 3: N = 1,000,000 Clustered Gaussian Distribution", data);
    }

    return 0;
}
