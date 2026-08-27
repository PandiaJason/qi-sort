#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <numeric>
#include <string>
#include <cstring>

// Include ALL QI Family Models
#include "../include/qi_radix.hpp"
#include "../include/qi_apex.hpp"
#include "../research/qi_field_sort.hpp"
#include "../research/qi_wave_sort.hpp"
#include "../research/qi_partition_sort.hpp"
#include "../research/qi_turbo_radix.hpp"

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

struct EngineResult {
    string name;
    string modelType;
    double timeMs;
    double mkeysPerSec;
    bool passed;
};

void runQiFamilyArena(const string& title, const vector<u32>& original) {
    const size_t N = original.size();
    cout << "========================================================================================\n";
    cout << "  ARENA: " << title << " (N = " << N << " elements)\n";
    cout << "========================================================================================\n";
    cout << left << setw(34) << "QI Model / Engine"
         << setw(26) << "Algorithmic Family"
         << setw(14) << "Time (ms)"
         << setw(20) << "MKeys/s"
         << "Status\n";
    cout << "----------------------------------------------------------------------------------------\n";

    vector<EngineResult> results;

    // 0. std::sort Baseline
    {
        auto data = original;
        double t = time_ms([&]() { data = original; sort(data.begin(), data.end()); });
        bool ok = is_sorted(data.begin(), data.end());
        double mkeys = (N / 1e6) / (t / 1000.0);
        results.push_back({"std::sort (Baseline)", "C++ Introsort", t, mkeys, ok});
    }

    // 1. qi::sort (v0.3.61 Production)
    {
        auto data = original;
        double t = time_ms([&]() { data = original; qi::sort(data.data(), N); });
        bool ok = is_sorted(data.begin(), data.end());
        double mkeys = (N / 1e6) / (t / 1000.0);
        results.push_back({"qi::sort (v0.3.61)", "Adaptive Radix", t, mkeys, ok});
    }

    // 2. qi_turbo (4-Banked Quad-Pipelined Radix)
    {
        auto data = original;
        double t = time_ms([&]() { data = original; qi_turbo::sort(data.data(), N); });
        bool ok = is_sorted(data.begin(), data.end());
        double mkeys = (N / 1e6) / (t / 1000.0);
        results.push_back({"qi_turbo (Research)", "4-Banked Radix-11", t, mkeys, ok});
    }

    // 3. QI Partition Sort (Fixed-Point Micro-Buckets)
    {
        auto data = original;
        double t = time_ms([&]() { data = original; qi_partition::sort(data.data(), N); });
        bool ok = is_sorted(data.begin(), data.end());
        double mkeys = (N / 1e6) / (t / 1000.0);
        results.push_back({"QI Partition Sort", "Micro-Bucket Q32.32", t, mkeys, ok});
    }

    // 4. QI-FieldSort (Continuous Density Field Inversion)
    {
        auto data = original;
        double t = time_ms([&]() { data = original; qi_field::sort(data.data(), N); });
        bool ok = is_sorted(data.begin(), data.end());
        double mkeys = (N / 1e6) / (t / 1000.0);
        results.push_back({"QI-FieldSort (Original)", "Density Field Inversion", t, mkeys, ok});
    }

    // 5. QI-WaveSort (Wavefunction Block Cache)
    {
        auto data = original;
        double t = time_ms([&]() { data = original; qi_wave::sort(data.data(), N); });
        bool ok = is_sorted(data.begin(), data.end());
        double mkeys = (N / 1e6) / (t / 1000.0);
        results.push_back({"QI-WaveSort (Research)", "Wave Block Cache", t, mkeys, ok});
    }

    // 6. qi::apex (Ultimate Universal Engine)
    {
        auto data = original;
        double t = time_ms([&]() { data = original; qi::apex::sort(data.data(), N); });
        bool ok = is_sorted(data.begin(), data.end());
        double mkeys = (N / 1e6) / (t / 1000.0);
        results.push_back({"qi::apex (Ultimate)", "Universal Adaptive Apex", t, mkeys, ok});
    }

    for (const auto& r : results) {
        cout << left << setw(34) << r.name
             << setw(26) << r.modelType
             << setw(14) << fixed << setprecision(2) << r.timeMs
             << setw(20) << r.mkeysPerSec
             << (r.passed ? "PASS" : "FAIL") << "\n";
    }
    cout << "----------------------------------------------------------------------------------------\n\n";
}

int main() {
    cout << "========================================================================================\n";
    cout << "  THE QI FAMILY SHOWDOWN: COMPARING EVERY QI MODEL EVER CREATED\n";
    cout << "  Hardware: Apple Silicon M1 Pro | clang++ -O3 -std=c++17 | Best of 5 Runs\n";
    cout << "========================================================================================\n\n";

    mt19937_64 rng(1337);

    // 1. N = 100,000 Keys
    {
        size_t N = 100000;
        vector<u32> data(N);
        for (auto& x : data) x = rng();
        runQiFamilyArena("100,000 Uniform Random Keys", data);
    }

    // 2. N = 1,000,000 Keys
    {
        size_t N = 1000000;
        vector<u32> data(N);
        for (auto& x : data) x = rng();
        runQiFamilyArena("1,000,000 Uniform Random Keys", data);
    }

    // 3. N = 10,000,000 Keys
    {
        size_t N = 10000000;
        vector<u32> data(N);
        for (auto& x : data) x = rng();
        runQiFamilyArena("10,000,000 Uniform Random Keys", data);
    }

    // 4. N = 10,000,000 Narrow Domain (0-255)
    {
        size_t N = 10000000;
        vector<u32> data(N);
        uniform_int_distribution<u32> dist(0, 255);
        for (auto& x : data) x = dist(rng);
        runQiFamilyArena("10,000,000 Low-Range Keys (0-255)", data);
    }

    // 5. N = 10,000,000 16-bit Domain (0-65535)
    {
        size_t N = 10000000;
        vector<u32> data(N);
        uniform_int_distribution<u32> dist(0, 65535);
        for (auto& x : data) x = dist(rng);
        runQiFamilyArena("10,000,000 Medium Domain Keys (0-65535)", data);
    }

    return 0;
}
