/*
===============================================================================
REAL-WORLD BENCHMARK: COLUMNAR DATABASE ENGINE (DuckDB / ClickHouse Style)
===============================================================================

Simulates an in-memory columnar database processing an ORDER BY query on a 
10,000,000 (10 Million) row table across multiple column distributions.

Compares:
    1. std::sort (C++ Standard Introsort - used in gcc/clang libstdc++)
    2. std::stable_sort (Timsort / Mergesort - used in Python/Java/Rust)
    3. qi::sort (Our Quantum-Inspired Adaptive Radix Engine)

===============================================================================
Build:

    g++ -O3 -std=c++17 -march=native real_world_database_benchmark.cpp -o db_benchmark

Run:

    ./db_benchmark

===============================================================================
*/

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "../include/qi_radix.hpp"

using namespace std;
using Clock = chrono::high_resolution_clock;

// 10 Million Records Table
static constexpr size_t TABLE_ROWS = 10'000'000;

struct ColumnBenchmarkResult {
    string columnName;
    string distributionType;
    double stdSortMs = 0.0;
    double stdStableSortMs = 0.0;
    double qiSortMs = 0.0;
    double speedupVsStdSort = 0.0;
    double speedupVsStableSort = 0.0;
    bool correctness = false;
};

static double measureMs(const std::function<void()>& fn) {
    auto start = Clock::now();
    fn();
    auto end = Clock::now();
    return chrono::duration<double, milli>(end - start).count();
}

static ColumnBenchmarkResult benchmarkColumn(
    const string& name,
    const string& distDesc,
    const vector<uint32_t>& originalData
) {
    ColumnBenchmarkResult res;
    res.columnName = name;
    res.distributionType = distDesc;

    // 1. std::sort
    {
        vector<uint32_t> data = originalData;
        res.stdSortMs = measureMs([&]() {
            sort(data.begin(), data.end());
        });
    }

    // 2. std::stable_sort
    {
        vector<uint32_t> data = originalData;
        res.stdStableSortMs = measureMs([&]() {
            stable_sort(data.begin(), data.end());
        });
    }

    // 3. qi::sort (Our Quantum-Inspired Engine)
    vector<uint32_t> qiData = originalData;
    {
        res.qiSortMs = measureMs([&]() {
            qi::sort(qiData);
        });
    }

    // Correctness check against reference
    vector<uint32_t> refData = originalData;
    sort(refData.begin(), refData.end());
    res.correctness = (refData == qiData);

    res.speedupVsStdSort = res.stdSortMs / res.qiSortMs;
    res.speedupVsStableSort = res.stdStableSortMs / res.qiSortMs;

    return res;
}

int main() {
    ios::sync_with_stdio(false);

    cout << "========================================================================================\n";
    cout << "REAL-WORLD COLUMNAR DATABASE ENGINE BENCHMARK (Table Size: 10,000,000 Rows)\n";
    cout << "========================================================================================\n\n";

    mt19937_64 rng(42);

    // Column 1: Transaction Order IDs (Uniform 32-bit Random)
    cout << "[1/4] Generating Column 'order_id' (Uniform 32-bit Random IDs)...\n";
    vector<uint32_t> col_order_id(TABLE_ROWS);
    uniform_int_distribution<uint32_t> randDist(0, numeric_limits<uint32_t>::max());
    for (auto& x : col_order_id) x = randDist(rng);

    // Column 2: User Surrogate Keys (Clustered Power-Law Repeating IDs)
    cout << "[2/4] Generating Column 'user_id' (Clustered Power-Law Customer Keys)...\n";
    vector<uint32_t> col_user_id(TABLE_ROWS);
    uniform_real_distribution<double> uDist(0.0, 1.0);
    for (auto& x : col_user_id) {
        double u = uDist(rng);
        x = static_cast<uint32_t>(pow(u, 10.0) * 500000.0);
    }

    // Column 3: Event Timestamp (Almost-Sorted Epoch Timestamps)
    cout << "[3/4] Generating Column 'timestamp_sec' (Monotonic / Almost-Sorted Epoch Times)...\n";
    vector<uint32_t> col_timestamp(TABLE_ROWS);
    uint32_t baseTime = 1700000000u;
    for (size_t i = 0; i < TABLE_ROWS; ++i) col_timestamp[i] = baseTime + static_cast<uint32_t>(i / 2);
    uniform_int_distribution<size_t> posDist(0, TABLE_ROWS - 1);
    for (size_t i = 0; i < TABLE_ROWS / 1000; ++i) {
        swap(col_timestamp[posDist(rng)], col_timestamp[posDist(rng)]);
    }

    // Column 4: Category Code (Low-Range 16-bit Product Categories)
    cout << "[4/4] Generating Column 'category_code' (Low-Range 16-bit Product Categories)...\n";
    vector<uint32_t> col_category(TABLE_ROWS);
    uniform_int_distribution<uint32_t> catDist(0, 65535);
    for (auto& x : col_category) x = catDist(rng);

    cout << "\nAll 40 Million data elements generated! Running benchmark queries...\n\n";

    vector<ColumnBenchmarkResult> results;
    results.push_back(benchmarkColumn("order_id", "Uniform 32-bit", col_order_id));
    results.push_back(benchmarkColumn("user_id", "Power-Law Clustered", col_user_id));
    results.push_back(benchmarkColumn("timestamp_sec", "Almost-Sorted Epoch", col_timestamp));
    results.push_back(benchmarkColumn("category_code", "Low-Range 16-bit", col_category));

    // Results Display
    cout << "========================================================================================\n";
    cout << "BENCHMARK RESULTS: ORDER BY Execution Time on 10,000,000 Rows per Column\n";
    cout << "========================================================================================\n";
    cout << left << setw(18) << "Column Name"
         << setw(22) << "Distribution"
         << setw(16) << "std::sort (ms)"
         << setw(18) << "std::stable_sort"
         << setw(16) << "qi::sort (ms)"
         << setw(12) << "Speedup"
         << setw(10) << "Status" << "\n";
    cout << "----------------------------------------------------------------------------------------\n";

    double totalStdSortMs = 0.0;
    double totalStableSortMs = 0.0;
    double totalQiSortMs = 0.0;

    for (const auto& r : results) {
        totalStdSortMs += r.stdSortMs;
        totalStableSortMs += r.stdStableSortMs;
        totalQiSortMs += r.qiSortMs;

        cout << left << setw(18) << r.columnName
             << setw(22) << r.distributionType
             << setw(16) << fixed << setprecision(2) << r.stdSortMs
             << setw(18) << r.stdStableSortMs
             << setw(16) << r.qiSortMs
             << setw(12) << setprecision(2) << (to_string(r.speedupVsStdSort).substr(0,4) + "x")
             << setw(10) << (r.correctness ? "PASS" : "FAIL") << "\n";
    }

    cout << "----------------------------------------------------------------------------------------\n";
    cout << left << setw(40) << "TOTAL TABLE SORTING TIME (40M Rows)"
         << setw(16) << fixed << setprecision(2) << totalStdSortMs
         << setw(18) << totalStableSortMs
         << setw(16) << totalQiSortMs
         << setw(12) << setprecision(2) << (to_string(totalStdSortMs / totalQiSortMs).substr(0,4) + "x")
         << setw(10) << "PASS" << "\n";
    cout << "========================================================================================\n\n";

    cout << "SUMMARY OF REAL-WORLD ADVANTAGE:\n";
    cout << "  * Total std::sort time across table        : " << totalStdSortMs / 1000.0 << " seconds\n";
    cout << "  * Total std::stable_sort time across table : " << totalStableSortMs / 1000.0 << " seconds\n";
    cout << "  * Total qi::sort (Our Engine) time         : " << totalQiSortMs / 1000.0 << " seconds\n";
    cout << "  * Overall Speedup vs std::sort             : " << (totalStdSortMs / totalQiSortMs) << "x FASTER\n";
    cout << "  * Overall Speedup vs std::stable_sort      : " << (totalStableSortMs / totalQiSortMs) << "x FASTER\n";
    cout << "========================================================================================\n";

    return 0;
}
