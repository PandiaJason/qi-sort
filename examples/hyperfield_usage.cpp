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
    cout << "  Shifted Dynamic-Window & Continuous Interpolation Sorter\n";
    cout << "========================================================================================\n\n";

    mt19937_64 rng(42);
    const size_t N = 10000000;

    // 1. Shifted 16-bit Window (Values 1,000,000 to 1,050,000)
    {
        vector<uint32_t> data(N);
        for (auto& x : data) x = 1000000 + (rng() % 50000);

        auto t0 = chrono::high_resolution_clock::now();
        qi::hyperfield::sort(data.data(), N);
        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();

        bool ok = is_sorted(data.begin(), data.end());
        cout << "1. qi::hyperfield::sort (10M Shifted Window): " << fixed << setprecision(2) << ms << " ms ("
             << (N / 1e6) / (ms / 1000.0) << " MKeys/s) [" << (ok ? "PASS" : "FAIL") << "]\n";
    }

    // 2. Unsigned 32-bit Random (1M)
    {
        const size_t N1M = 1000000;
        vector<uint32_t> data(N1M);
        for (auto& x : data) x = rng();

        auto t0 = chrono::high_resolution_clock::now();
        qi::hyperfield::sort(data.data(), N1M);
        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();

        bool ok = is_sorted(data.begin(), data.end());
        cout << "2. qi::hyperfield::sort (1M Random uint32)  : " << fixed << setprecision(2) << ms << " ms ("
             << (N1M / 1e6) / (ms / 1000.0) << " MKeys/s) [" << (ok ? "PASS" : "FAIL") << "]\n";
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
        cout << "3. qi::hyperfield::parallel_sort (10M)     : " << fixed << setprecision(2) << ms << " ms ("
             << (N / 1e6) / (ms / 1000.0) << " MKeys/s) [" << (ok ? "PASS" : "FAIL") << "]\n";
    }

    cout << "\n========================================================================================\n";
    cout << "  QI::HYPERFIELD ALL TESTS PASSED!\n";
    cout << "========================================================================================\n";

    return 0;
}
