#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include "../include/qi_apex.hpp"
#include "qi_hyper_apex.hpp"

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

void testSize(size_t N, mt19937_64& rng) {
    cout << "========================================================================================\n";
    cout << "  SHOWDOWN: N = " << N << " Elements (Uniform Random 32-bit)\n";
    cout << "========================================================================================\n";
    cout << left << setw(38) << "Engine"
         << setw(16) << "Time (ms)"
         << setw(24) << "Throughput (MKeys/s)"
         << "Status\n";
    cout << "----------------------------------------------------------------------------------------\n";

    vector<u32> original(N);
    for (auto& x : original) x = rng();

    // 1. std::sort
    {
        auto data = original;
        double t = time_ms([&]() { data = original; sort(data.data(), data.data() + N); });
        bool ok = is_sorted(data.begin(), data.end());
        cout << left << setw(38) << "std::sort (C++ Baseline)"
             << setw(16) << fixed << setprecision(2) << t
             << setw(24) << (N / 1e6) / (t / 1000.0)
             << (ok ? "PASS" : "FAIL") << "\n";
    }

    // 2. qi::apex
    {
        auto data = original;
        double t = time_ms([&]() { data = original; qi::apex::sort(data.data(), N); });
        bool ok = is_sorted(data.begin(), data.end());
        cout << left << setw(38) << "★ qi::apex (Current Champion)"
             << setw(16) << fixed << setprecision(2) << t
             << setw(24) << (N / 1e6) / (t / 1000.0)
             << (ok ? "PASS" : "FAIL") << "\n";
    }

    // 3. qi::hyper_apex
    {
        auto data = original;
        double t = time_ms([&]() { data = original; qi::hyper_apex::sort(data.data(), N); });
        bool ok = is_sorted(data.begin(), data.end());
        cout << left << setw(38) << "⚡ qi::hyper_apex (2-Pass Zero-Memcpy)"
             << setw(16) << fixed << setprecision(2) << t
             << setw(24) << (N / 1e6) / (t / 1000.0)
             << (ok ? "PASS" : "FAIL") << "\n";
    }

    cout << "----------------------------------------------------------------------------------------\n\n";
}

int main() {
    cout << "========================================================================================\n";
    cout << "  THE NEXT FRONTIER: qi::apex vs qi::hyper_apex SHOWDOWN\n";
    cout << "  Hardware: Apple Silicon M1 Pro | clang++ -O3 -std=c++17 | Best of 5 Runs\n";
    cout << "========================================================================================\n\n";

    mt19937_64 rng(42);

    testSize(1000000, rng);
    testSize(5000000, rng);
    testSize(10000000, rng);
    testSize(20000000, rng);

    return 0;
}
