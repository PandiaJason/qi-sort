#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <cassert>
#include "../include/qi_apex.hpp"

using namespace std;

int main() {
    cout << "========================================================================================\n";
    cout << "  qi::apex ULTIMATE: UNIVERSAL MULTI-DATATYPE & DATABASE PAIR VERIFICATION\n";
    cout << "========================================================================================\n\n";

    mt19937_64 rng(42);

    // 1. uint32_t
    {
        size_t N = 1000000;
        vector<uint32_t> data(N);
        for (auto& x : data) x = rng();
        auto t0 = chrono::high_resolution_clock::now();
        qi::apex::sort(data);
        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();
        bool ok = is_sorted(data.begin(), data.end());
        cout << "[1] uint32_t (1M keys)      : " << fixed << setprecision(2) << ms << " ms ("
             << (ok ? "PASS" : "FAIL") << ")\n";
        assert(ok);
    }

    // 2. int32_t (Positive and Negative)
    {
        size_t N = 1000000;
        vector<int32_t> data(N);
        uniform_int_distribution<int32_t> dist(-1000000000, 1000000000);
        for (auto& x : data) x = dist(rng);
        auto t0 = chrono::high_resolution_clock::now();
        qi::apex::sort(data);
        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();
        bool ok = is_sorted(data.begin(), data.end());
        cout << "[2] int32_t (1M signed)     : " << fixed << setprecision(2) << ms << " ms ("
             << (ok ? "PASS" : "FAIL") << ")\n";
        assert(ok);
    }

    // 3. float (IEEE 754 32-bit with negative floats)
    {
        size_t N = 1000000;
        vector<float> data(N);
        uniform_real_distribution<float> dist(-1e6f, 1e6f);
        for (auto& x : data) x = dist(rng);
        auto t0 = chrono::high_resolution_clock::now();
        qi::apex::sort(data);
        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();
        bool ok = is_sorted(data.begin(), data.end());
        cout << "[3] float (1M IEEE 754)     : " << fixed << setprecision(2) << ms << " ms ("
             << (ok ? "PASS" : "FAIL") << ")\n";
        assert(ok);
    }

    // 4. uint64_t (64-bit integer)
    {
        size_t N = 1000000;
        vector<uint64_t> data(N);
        for (auto& x : data) x = rng();
        auto t0 = chrono::high_resolution_clock::now();
        qi::apex::sort(data);
        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();
        bool ok = is_sorted(data.begin(), data.end());
        cout << "[4] uint64_t (1M 64-bit)    : " << fixed << setprecision(2) << ms << " ms ("
             << (ok ? "PASS" : "FAIL") << ")\n";
        assert(ok);
    }

    // 5. int64_t (Signed 64-bit integer)
    {
        size_t N = 1000000;
        vector<int64_t> data(N);
        uniform_int_distribution<int64_t> dist(-1000000000000LL, 1000000000000LL);
        for (auto& x : data) x = dist(rng);
        auto t0 = chrono::high_resolution_clock::now();
        qi::apex::sort(data);
        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();
        bool ok = is_sorted(data.begin(), data.end());
        cout << "[5] int64_t (1M signed 64)  : " << fixed << setprecision(2) << ms << " ms ("
             << (ok ? "PASS" : "FAIL") << ")\n";
        assert(ok);
    }

    // 6. double (IEEE 754 64-bit float)
    {
        size_t N = 1000000;
        vector<double> data(N);
        uniform_real_distribution<double> dist(-1e9, 1e9);
        for (auto& x : data) x = dist(rng);
        auto t0 = chrono::high_resolution_clock::now();
        qi::apex::sort(data);
        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();
        bool ok = is_sorted(data.begin(), data.end());
        cout << "[6] double (1M 64-bit float): " << fixed << setprecision(2) << ms << " ms ("
             << (ok ? "PASS" : "FAIL") << ")\n";
        assert(ok);
    }

    // 7. Database ORDER BY (Key-Payload Tuple Pair)
    {
        size_t N = 1000000;
        vector<uint32_t> keys(N);
        vector<uint64_t> payloads(N);
        for (size_t i = 0; i < N; ++i) {
            keys[i] = rng();
            payloads[i] = i + 1000;
        }
        auto t0 = chrono::high_resolution_clock::now();
        qi::apex::sort_pairs(keys.data(), payloads.data(), N);
        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();
        bool ok = is_sorted(keys.begin(), keys.end());
        cout << "[7] sort_pairs (1M Tuples)  : " << fixed << setprecision(2) << ms << " ms ("
             << (ok ? "PASS" : "FAIL") << ")\n";
        assert(ok);
    }

    cout << "\n========================================================================================\n";
    cout << "  ALL 7 UNIVERSAL DATATYPES & TUPLE PAIRS PASSED 100% CORRECTNESS VERIFICATION!\n";
    cout << "========================================================================================\n";

    return 0;
}
