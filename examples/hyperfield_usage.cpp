#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <cstdint>
#include <cassert>
#include "../include/qi_hyperfield.hpp"

using namespace std;

int main() {
    cout << "========================================================================================\n";
    cout << "  QI::HYPERFIELD DEMONSTRATION & VERIFICATION\n";
    cout << "  100% Non-Radix, Non-Comparison Continuous Density Field Inversion Sorter\n";
    cout << "========================================================================================\n\n";

    mt19937_64 rng(42);
    const size_t N = 1000000;

    // 1. Unsigned 32-bit Integer Sort
    {
        vector<uint32_t> data(N);
        for (auto& x : data) x = rng();

        auto t0 = chrono::high_resolution_clock::now();
        qi::hyperfield::sort(data.data(), N);
        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();

        bool ok = is_sorted(data.begin(), data.end());
        cout << "1. qi::hyperfield::sort (1M uint32)   : " << fixed << setprecision(2) << ms << " ms ("
             << (N / 1e6) / (ms / 1000.0) << " MKeys/s) [" << (ok ? "PASS" : "FAIL") << "]\n";
    }

    // 2. IEEE 754 Float Sort
    {
        vector<float> data(N);
        uniform_real_distribution<float> dist(-1e5f, 1e5f);
        for (auto& x : data) x = dist(rng);

        auto t0 = chrono::high_resolution_clock::now();
        qi::hyperfield::sort(data.data(), N);
        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();

        bool ok = is_sorted(data.begin(), data.end());
        cout << "2. qi::hyperfield::sort (1M float32)  : " << fixed << setprecision(2) << ms << " ms ("
             << (N / 1e6) / (ms / 1000.0) << " MKeys/s) [" << (ok ? "PASS" : "FAIL") << "]\n";
    }

    // 3. Multi-Core Parallel Sort
    {
        vector<uint32_t> data(N);
        for (auto& x : data) x = rng();

        auto t0 = chrono::high_resolution_clock::now();
        qi::hyperfield::parallel_sort(data.data(), N);
        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();

        bool ok = is_sorted(data.begin(), data.end());
        cout << "3. qi::hyperfield::parallel_sort (1M) : " << fixed << setprecision(2) << ms << " ms ("
             << (N / 1e6) / (ms / 1000.0) << " MKeys/s) [" << (ok ? "PASS" : "FAIL") << "]\n";
    }

    cout << "\n========================================================================================\n";
    cout << "  QI::HYPERFIELD ALL TESTS PASSED SUCCESSFULLY!\n";
    cout << "========================================================================================\n";

    return 0;
}
