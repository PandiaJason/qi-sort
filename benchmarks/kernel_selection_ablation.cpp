/*
================================================================
KERNEL SELECTION ABLATION & REGRET ANALYSIS
================================================================
Tests whether QI-Sort's quantum-inspired state sensing mechanism
correctly predicts the optimal radix kernel across diverse data distributions.

Evaluates:
  1. Fixed Radix-8  (4 passes, 256 buckets)
  2. Fixed Radix-11 (3 passes, 2048 buckets)
  3. Fixed Radix-16 (2 passes, 65536 buckets)
  4. QI-Sort (Adaptive sensing auto-selection)
  5. Oracle Best = min(T_R8, T_R11, T_R16)

Regret Metric:
  Regret = (T_QI - T_oracle) / T_oracle

A Regret close to 0 (or low single digits %) proves that QI-Sort's
sensing mechanism reliably matches or approaches the Oracle Best choice.
================================================================
*/

#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>
#include <iomanip>
#include <numeric>
#include <cmath>
#include <string>
#include <functional>

#include "../include/qi_radix.hpp"

using namespace std;
using Clock = chrono::high_resolution_clock;

// Direct invocation of individual fixed kernels without sensing
static void run_fixed_r8(uint32_t* data, size_t n) {
    qi::detail::radixSort8(data, n, false);
}

static void run_fixed_r11(uint32_t* data, size_t n) {
    qi::detail::radixSort11(data, n, false);
}

static void run_fixed_r16(uint32_t* data, size_t n) {
    qi::detail::radixSort16(data, n, false);
}

static inline double measure_ms(function<void()> fn, int runs = 5) {
    double min_ms = 1e18;
    for (int r = 0; r < runs; r++) {
        auto start = Clock::now();
        fn();
        double ms = chrono::duration<double, milli>(Clock::now() - start).count();
        min_ms = min(min_ms, ms);
    }
    return min_ms;
}

struct TestResult {
    string name;
    qi::Radix selectedRadix;
    double t_r8;
    double t_r11;
    double t_r16;
    double t_qi; // includes sensing overhead
    double t_oracle;
    qi::Radix oracleRadix;
    double regretPct;
    bool selectedIsOracle;
};

int main() {
    const size_t N = 5000000; // 5M items for fast, accurate measurements
    cout << "\n================================================================\n";
    cout << "  KERNEL SELECTION ABLATION & REGRET ANALYSIS (N = " << N << ")\n";
    cout << "  Testing Adaptive QI Sensing vs Fixed Radix-8, 11, 16 & Oracle\n";
    cout << "================================================================\n\n";

    vector<pair<string, vector<uint32_t>>> datasets;

    // 1. Uniform Random 32-bit (High Entropy)
    {
        mt19937_64 rng(42);
        vector<uint32_t> d(N);
        uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
        for (auto& x : d) x = dist(rng);
        datasets.push_back({"Uniform Random 32-bit", d});
    }

    // 2. Small Range 16-bit (High bits zeroed)
    {
        mt19937_64 rng(101);
        vector<uint32_t> d(N);
        uniform_int_distribution<uint32_t> dist(0, 65535);
        for (auto& x : d) x = dist(rng);
        datasets.push_back({"Small Range 16-bit", d});
    }

    // 3. Medium Range 20-bit
    {
        mt19937_64 rng(202);
        vector<uint32_t> d(N);
        uniform_int_distribution<uint32_t> dist(0, (1u << 20) - 1);
        for (auto& x : d) x = dist(rng);
        datasets.push_back({"Medium Range 20-bit", d});
    }

    // 4. Duplicate Heavy (95% Duplicates)
    {
        mt19937_64 rng(303);
        vector<uint32_t> d(N);
        uniform_int_distribution<uint32_t> dist(0, 100);
        for (auto& x : d) x = dist(rng);
        datasets.push_back({"Duplicate Heavy (95%+ Dups)", d});
    }

    // 5. Clustered Gauissian / Mixture
    {
        mt19937_64 rng(404);
        vector<uint32_t> d(N);
        normal_distribution<double> n1(1000000.0, 50000.0);
        normal_distribution<double> n2(5000000.0, 200000.0);
        for (size_t i = 0; i < N; i++) {
            double v = (i % 2 == 0) ? n1(rng) : n2(rng);
            d[i] = static_cast<uint32_t>(max(0.0, v));
        }
        datasets.push_back({"Clustered Bimodal", d});
    }

    // 6. Dense Sequential with Small Noise
    {
        mt19937_64 rng(505);
        vector<uint32_t> d(N);
        for (size_t i = 0; i < N; i++) {
            d[i] = static_cast<uint32_t>(i + (rng() % 100));
        }
        datasets.push_back({"Dense Sequential + Noise", d});
    }

    // 7. Byte-Shifted Entropy (High byte active, middle bytes low entropy)
    {
        mt19937_64 rng(606);
        vector<uint32_t> d(N);
        for (size_t i = 0; i < N; i++) {
            uint32_t b0 = rng() % 256;
            uint32_t b3 = rng() % 256;
            d[i] = (b3 << 24) | (0x12 << 16) | (0x34 << 8) | b0;
        }
        datasets.push_back({"Byte-Shifted Entropy", d});
    }

    vector<TestResult> results;

    for (const auto& [name, orig_data] : datasets) {
        // Sensing inspection
        qi::State state = qi::analyze(orig_data);
        qi::Radix selected = state.recommendedRadix;

        // Measure R8
        double t_r8 = measure_ms([&]() {
            auto work = orig_data;
            run_fixed_r8(work.data(), work.size());
        });

        // Measure R11
        double t_r11 = measure_ms([&]() {
            auto work = orig_data;
            run_fixed_r11(work.data(), work.size());
        });

        // Measure R16
        double t_r16 = measure_ms([&]() {
            auto work = orig_data;
            run_fixed_r16(work.data(), work.size());
        });

        // Measure QI-Sort (full pipeline: sensing + dispatch + execution)
        qi::SortOptions opts;
        opts.allowShortcuts = false; // Disable short-circuit so we test pure radix selection
        double t_qi = measure_ms([&]() {
            auto work = orig_data;
            qi::sort(work.data(), work.size(), opts);
        });

        // Oracle determination
        double t_oracle = t_r16;
        qi::Radix oracleRadix = qi::Radix::R16;
        if (t_r11 < t_oracle) { t_oracle = t_r11; oracleRadix = qi::Radix::R11; }
        if (t_r8 < t_oracle) { t_oracle = t_r8; oracleRadix = qi::Radix::R8; }

        double regret = ((t_qi - t_oracle) / t_oracle) * 100.0;
        if (regret < 0.0) regret = 0.0; // In case QI sensing optimization/inlining beat bare call

        results.push_back({
            name,
            selected,
            t_r8,
            t_r11,
            t_r16,
            t_qi,
            t_oracle,
            oracleRadix,
            regret,
            (selected == oracleRadix)
        });
    }

    // Output Table
    auto rstr = [](qi::Radix r) {
        switch(r) {
            case qi::Radix::R8: return "R8";
            case qi::Radix::R11: return "R11";
            case qi::Radix::R16: return "R16";
        }
        return "R16";
    };

    cout << left << setw(30) << "Distribution"
         << setw(10) << "Sensed"
         << setw(10) << "Oracle"
         << setw(12) << "T_R8 (ms)"
         << setw(12) << "T_R11 (ms)"
         << setw(12) << "T_R16 (ms)"
         << setw(12) << "T_QI (ms)"
         << setw(14) << "T_Oracle(ms)"
         << setw(12) << "Regret (%)"
         << "\n";
    cout << string(134, '-') << "\n";

    double totalRegret = 0.0;
    int oracleMatches = 0;

    for (const auto& r : results) {
        cout << left << setw(30) << r.name
             << setw(10) << rstr(r.selectedRadix)
             << setw(10) << rstr(r.oracleRadix)
             << setw(12) << fixed << setprecision(2) << r.t_r8
             << setw(12) << fixed << setprecision(2) << r.t_r11
             << setw(12) << fixed << setprecision(2) << r.t_r16
             << setw(12) << fixed << setprecision(2) << r.t_qi
             << setw(14) << fixed << setprecision(2) << r.t_oracle
             << setw(12) << fixed << setprecision(2) << r.regretPct << " %"
             << (r.selectedIsOracle ? " [EXACT ORACLE]" : "")
             << "\n";

        totalRegret += r.regretPct;
        if (r.selectedIsOracle) oracleMatches++;
    }

    cout << string(134, '-') << "\n";
    double avgRegret = totalRegret / results.size();
    cout << "\nSummary Metrics:\n";
    cout << "  - Oracle Selection Match Rate: " << oracleMatches << " / " << results.size()
         << " (" << fixed << setprecision(1) << (100.0 * oracleMatches / results.size()) << "%)\n";
    cout << "  - Mean Selection Regret: " << fixed << setprecision(2) << avgRegret << " %\n";
    cout << "================================================================\n\n";

    return 0;
}
