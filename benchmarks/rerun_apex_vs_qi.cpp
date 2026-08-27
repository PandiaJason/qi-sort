#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <numeric>
#include <string>
#include <cstring>

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

struct RunResult {
    string name;
    double timeMs;
    double mkeysPerSec;
    bool ok;
};

void runComparison(const string& title, const vector<u32>& original, bool testParallel = true) {
    const size_t N = original.size();
    cout << "========================================================================================\n";
    cout << "  DATASET: " << title << " (N = " << N << " elements)\n";
    cout << "========================================================================================\n";
    cout << left << setw(40) << "Engine"
         << setw(16) << "Time (ms)"
         << setw(24) << "Throughput (MKeys/s)"
         << "Status\n";
    cout << "----------------------------------------------------------------------------------------\n";

    vector<RunResult> results;

    // 1. std::sort
    {
        auto data = original;
        double t = time_ms([&]() { data = original; sort(data.begin(), data.end()); });
        bool ok = is_sorted(data.begin(), data.end());
        double mkeys = (N / 1e6) / (t / 1000.0);
        results.push_back({"std::sort (C++ Introsort)", t, mkeys, ok});
    }

    // 2. qi::sort (v0.3.61 Baseline)
    {
        auto data = original;
        double t = time_ms([&]() { data = original; qi::sort(data.data(), N); });
        bool ok = is_sorted(data.begin(), data.end());
        double mkeys = (N / 1e6) / (t / 1000.0);
        results.push_back({"qi::sort (v0.3.61 Baseline)", t, mkeys, ok});
    }

    // 3. qi::apex (Single-Core)
    {
        auto data = original;
        double t = time_ms([&]() { data = original; qi::apex::sort(data.data(), N); });
        bool ok = is_sorted(data.begin(), data.end());
        double mkeys = (N / 1e6) / (t / 1000.0);
        results.push_back({"qi::apex (Single-Core)", t, mkeys, ok});
    }

    if (testParallel && N >= 100000) {
        // 4. qi::parallel_sort
        {
            auto data = original;
            double t = time_ms([&]() { data = original; qi::parallel_sort(data.data(), N); });
            bool ok = is_sorted(data.begin(), data.end());
            double mkeys = (N / 1e6) / (t / 1000.0);
            results.push_back({"qi::parallel_sort (Multi-Core)", t, mkeys, ok});
        }

        // 5. qi::apex::parallel_sort
        {
            auto data = original;
            double t = time_ms([&]() { data = original; qi::apex::parallel_sort(data.data(), N); });
            bool ok = is_sorted(data.begin(), data.end());
            double mkeys = (N / 1e6) / (t / 1000.0);
            results.push_back({"qi::apex::parallel_sort (Multi-Core)", t, mkeys, ok});
        }
    }

    for (const auto& r : results) {
        cout << left << setw(40) << r.name
             << setw(16) << fixed << setprecision(2) << r.timeMs
             << setw(24) << r.mkeysPerSec
             << (r.ok ? "PASS" : "FAIL") << "\n";
    }
    cout << "----------------------------------------------------------------------------------------\n\n";
}

int main() {
    cout << "========================================================================================\n";
    cout << "  HEAD-TO-HEAD SHOWDOWN: qi::apex vs qi::sort (v0.3.61) vs std::sort\n";
    cout << "  Hardware: Apple Silicon M1 Pro | clang++ -O3 -std=c++17 | Best of 5 Runs\n";
    cout << "========================================================================================\n\n";

    mt19937_64 rng(1337);

    // ── 1. N = 100,000 Uniform Random 32-bit ──
    {
        size_t N = 100000;
        vector<u32> data(N);
        for (size_t i = 0; i < N; ++i) data[i] = rng();
        runComparison("100K Uniform Random 32-bit Keys", data);
    }

    // ── 2. N = 1,000,000 Uniform Random 32-bit ──
    {
        size_t N = 1000000;
        vector<u32> data(N);
        for (size_t i = 0; i < N; ++i) data[i] = rng();
        runComparison("1M Uniform Random 32-bit Keys", data);
    }

    // ── 3. N = 5,000,000 Uniform Random 32-bit ──
    {
        size_t N = 5000000;
        vector<u32> data(N);
        for (size_t i = 0; i < N; ++i) data[i] = rng();
        runComparison("5M Uniform Random 32-bit Keys", data);
    }

    // ── 4. N = 10,000,000 Uniform Random 32-bit ──
    {
        size_t N = 10000000;
        vector<u32> data(N);
        for (size_t i = 0; i < N; ++i) data[i] = rng();
        runComparison("10M Uniform Random 32-bit Keys", data);
    }

    // ── 5. N = 10,000,000 Narrow Domain (Values 0-255) ──
    {
        size_t N = 10000000;
        vector<u32> data(N);
        uniform_int_distribution<u32> dist(0, 255);
        for (size_t i = 0; i < N; ++i) data[i] = dist(rng);
        runComparison("10M Narrow Domain Keys (Values 0-255)", data, false);
    }

    // ── 6. N = 10,000,000 Medium Domain (Values 0-65535) ──
    {
        size_t N = 10000000;
        vector<u32> data(N);
        uniform_int_distribution<u32> dist(0, 65535);
        for (size_t i = 0; i < N; ++i) data[i] = dist(rng);
        runComparison("10M Medium Domain Keys (Values 0-65535)", data, false);
    }

    // ── 7. N = 10,000,000 Nearly Sorted (95% Pre-Sorted) ──
    {
        size_t N = 10000000;
        vector<u32> data(N);
        iota(data.begin(), data.end(), 0);
        for (size_t i = 0; i < N / 20; ++i) {
            size_t a = rng() % N, b = rng() % N;
            swap(data[a], data[b]);
        }
        runComparison("10M Nearly Sorted Keys (95% Pre-Sorted)", data, false);
    }

    // ── 8. N = 10,000,000 Fully Sorted ──
    {
        size_t N = 10000000;
        vector<u32> data(N);
        for (size_t i = 0; i < N; ++i) data[i] = static_cast<u32>(i);
        runComparison("10M Fully Sorted Keys (Monotonic)", data, false);
    }

    return 0;
}
