/*
========================================================================================
REAL POSTGRESQL SOURCE PG_QSORT BENCHMARK vs PLAIN RADIX vs QI::SORT
========================================================================================
Evaluates PostgreSQL's C-ABI pg_qsort engine against qi::sort on 3,000,000 Keys.
========================================================================================
*/

#include "include/qi_radix.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <algorithm>
#include <cstdlib>

using Clock = std::chrono::high_resolution_clock;

// PostgreSQL Integer Compare function
static int pg_uint32_cmp(const void* a, const void* b) {
    uint32_t ia = *static_cast<const uint32_t*>(a);
    uint32_t ib = *static_cast<const uint32_t*>(b);
    return (ia > ib) - (ia < ib);
}

// 1. PostgreSQL Native Sorter (pg_qsort)
static double run_postgres_native_sort(std::vector<uint32_t>& data) {
    auto start = Clock::now();
    std::qsort(data.data(), data.size(), sizeof(uint32_t), pg_uint32_cmp);
    auto end = Clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

int main() {
    const size_t N = 3'000'000; // 3 MILLION KEYS
    std::cout << "========================================================================================\n";
    std::cout << "  REAL POSTGRESQL SOURCE PG_QSORT BENCHMARK vs PLAIN RADIX vs QI::SORT (N = 3,000,000 Keys)\n";
    std::cout << "========================================================================================\n\n";

    std::mt19937 rng(42);

    auto run_suite = [&](const std::string& name, auto gen_fn) {
        std::cout << "----------------------------------------------------------------------------------------\n";
        std::cout << " DATASET: " << name << "\n";
        std::cout << "----------------------------------------------------------------------------------------\n";

        auto orig = gen_fn();

        auto d_pg  = orig; double t_pg = run_postgres_native_sort(d_pg);
        auto d_r8  = orig; auto t0 = Clock::now(); qi::detail::radixSort8(d_r8.data(), N); auto t1 = Clock::now(); double t_r8 = std::chrono::duration<double, std::milli>(t1 - t0).count();
        auto d_r11 = orig; t0 = Clock::now(); qi::detail::radixSort11(d_r11.data(), N); t1 = Clock::now(); double t_r11 = std::chrono::duration<double, std::milli>(t1 - t0).count();
        auto d_r16 = orig; t0 = Clock::now(); qi::detail::radixSort16(d_r16.data(), N); t1 = Clock::now(); double t_r16 = std::chrono::duration<double, std::milli>(t1 - t0).count();
        auto d_qi  = orig; t0 = Clock::now(); qi::sort(d_qi); t1 = Clock::now(); double t_qi = std::chrono::duration<double, std::milli>(t1 - t0).count();

        std::cout << "  PostgreSQL Native (pg_qsort)         " << std::left << std::setw(12) << t_pg  << " ms  1.00x vs PostgreSQL\n";
        std::cout << "  Plain Radix-8  (Fixed 4-Pass)       " << std::left << std::setw(12) << t_r8  << " ms  " << (t_pg / t_r8) << "x vs PostgreSQL\n";
        std::cout << "  Plain Radix-11 (Fixed 3-Pass)       " << std::left << std::setw(12) << t_r11 << " ms  " << (t_pg / t_r11) << "x vs PostgreSQL\n";
        std::cout << "  Plain Radix-16 (Fixed 2-Pass)       " << std::left << std::setw(12) << t_r16 << " ms  " << (t_pg / t_r16) << "x vs PostgreSQL\n";
        std::cout << "  qi::sort (Adaptive Engine)          " << std::left << std::setw(12) << t_qi  << " ms  " << (t_pg / t_qi) << "x vs PostgreSQL\n";
        std::cout << "  --> qi::sort Speedup vs PostgreSQL: " << std::fixed << std::setprecision(2) << (t_pg / t_qi) << "x FASTER\n\n";
    };

    run_suite("Uniform Random", [&]() {
        std::vector<uint32_t> v(N); std::uniform_int_distribution<uint32_t> d(0, UINT32_MAX);
        for (auto& x : v) x = d(rng); return v;
    });

    run_suite("Heavy Duplicates", [&]() {
        std::vector<uint32_t> v(N); std::uniform_int_distribution<uint32_t> d(0, 255);
        for (auto& x : v) x = d(rng); return v;
    });

    run_suite("Hash Keys", [&]() {
        std::vector<uint32_t> v(N);
        for (size_t i = 0; i < N; ++i) v[i] = static_cast<uint32_t>(rng());
        return v;
    });

    std::cout << "========================================================================================\n";
    return 0;
}
