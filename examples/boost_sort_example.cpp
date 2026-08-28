#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include "../boost/sort/apex_sort/apex_sort.hpp"

using namespace std;

int main() {
    cout << "========================================================================================\n";
    cout << "  BOOST.SORT :: APEX_SORT STANDARD C++ DEMONSTRATION\n";
    cout << "  Interface: boost::sort::apex_sort(first, last)\n";
    cout << "========================================================================================\n\n";

    mt19937_64 rng(42);
    const size_t N = 10000000;

    // 1. Unsigned 32-bit Integer Sort
    {
        vector<uint32_t> data(N);
        for (auto& x : data) x = rng();

        auto t0 = chrono::high_resolution_clock::now();
        boost::sort::apex_sort(data.begin(), data.end());
        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();

        bool ok = is_sorted(data.begin(), data.end());
        cout << "1. boost::sort::apex_sort (10M uint32)   : " << fixed << setprecision(2) << ms << " ms ("
             << (N / 1e6) / (ms / 1000.0) << " MKeys/s) [" << (ok ? "PASS" : "FAIL") << "]\n";
    }

    // 2. IEEE 754 Float Sort
    {
        vector<float> data(N);
        uniform_real_distribution<float> dist(-1e5f, 1e5f);
        for (auto& x : data) x = dist(rng);

        auto t0 = chrono::high_resolution_clock::now();
        boost::sort::apex_sort(data.begin(), data.end());
        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();

        bool ok = is_sorted(data.begin(), data.end());
        cout << "2. boost::sort::apex_sort (10M float32)  : " << fixed << setprecision(2) << ms << " ms ("
             << (N / 1e6) / (ms / 1000.0) << " MKeys/s) [" << (ok ? "PASS" : "FAIL") << "]\n";
    }

    // 3. Multi-Core Parallel Sort
    {
        vector<uint32_t> data(N);
        for (auto& x : data) x = rng();

        auto t0 = chrono::high_resolution_clock::now();
        boost::sort::parallel_apex_sort(data.begin(), data.end());
        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();

        bool ok = is_sorted(data.begin(), data.end());
        cout << "3. boost::sort::parallel_apex_sort (10M) : " << fixed << setprecision(2) << ms << " ms ("
             << (N / 1e6) / (ms / 1000.0) << " MKeys/s) [" << (ok ? "PASS" : "FAIL") << "]\n";
    }

    cout << "\n========================================================================================\n";
    cout << "  BOOST.SORT APEX_SORT READY FOR FORMAL BOOST SUBMISSION!\n";
    cout << "========================================================================================\n";

    return 0;
}
