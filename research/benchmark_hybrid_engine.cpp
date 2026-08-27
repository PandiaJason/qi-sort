#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include "qi_hybrid_engine.hpp"
#include "../include/qi_radix.hpp"

template <typename Func>
double time_ms(Func f, int iterations = 5) {
    double best = 1e9;
    for (int i = 0; i < iterations; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        f();
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (ms < best) best = ms;
    }
    return best;
}

int main() {
    std::cout << "========================================================================================\n";
    std::cout << "  THE ULTIMATE HYBRID CHALLENGE: QI-HYBRID vs PRODUCTION QI::SORT\n";
    std::cout << "  Baseline: qi::sort (Production Radix Engine) = 1.00x\n";
    std::cout << "========================================================================================\n\n";

    std::vector<size_t> sizes = {100000, 1000000, 10000000};
    std::mt19937_64 rng(1337);

    for (size_t N : sizes) {
        std::cout << "--- Dataset Size: N = " << N << " Elements (Uniform Random 32-bit) ---\n";
        std::cout << std::left << std::setw(44) << "Algorithm / Engine"
                  << std::setw(16) << "Time (ms)"
                  << std::setw(24) << "vs qi::sort (Baseline)"
                  << "Status\n";
        std::cout << "----------------------------------------------------------------------------------------\n";

        std::vector<uint32_t> original(N);
        for (size_t i = 0; i < N; ++i) original[i] = rng();

        // 1. qi::sort (PRODUCTION RADIX BASELINE)
        double t_baseline = 0;
        {
            auto data = original;
            t_baseline = time_ms([&]() {
                data = original;
                qi::sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            std::cout << std::left << std::setw(44) << "qi::sort (Production Radix - BASELINE)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_baseline
                      << std::setw(24) << "1.00x (BASELINE)"
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 2. std::sort
        {
            auto data = original;
            double t_std = time_ms([&]() {
                data = original;
                std::sort(data.data(), data.data() + N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double ratio = t_std / t_baseline;
            std::cout << std::left << std::setw(44) << "std::sort (C++ Introsort)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_std
                      << std::setw(24) << (std::to_string(ratio).substr(0, 4) + "x SLOWER")
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 3. QI-Hybrid (Single-Core Challenger)
        {
            auto data = original;
            double t_hybrid = time_ms([&]() {
                data = original;
                qi_hybrid::sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double ratio = t_baseline / t_hybrid;
            std::string label = (ratio >= 1.0) ? (std::to_string(ratio).substr(0, 4) + "x FASTER") 
                                               : (std::to_string(1.0 / ratio).substr(0, 4) + "x slower");
            std::cout << std::left << std::setw(44) << "QI-Hybrid (Single-Core Engine)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_hybrid
                      << std::setw(24) << label
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 4. QI-Hybrid (Multi-Core Parallel Challenger)
        {
            auto data = original;
            double t_hybrid_par = time_ms([&]() {
                data = original;
                qi_hybrid::parallel_sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double ratio = t_baseline / t_hybrid_par;
            std::string label = (ratio >= 1.0) ? (std::to_string(ratio).substr(0, 4) + "x FASTER") 
                                               : (std::to_string(1.0 / ratio).substr(0, 4) + "x slower");
            std::cout << std::left << std::setw(44) << "QI-Hybrid (Multi-Core PARALLEL)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_hybrid_par
                      << std::setw(24) << label
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 5. qi::parallel_sort (Multi-Core Radix Reference)
        {
            auto data = original;
            double t_radix_par = time_ms([&]() {
                data = original;
                qi::parallel_sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double ratio = t_baseline / t_radix_par;
            std::cout << std::left << std::setw(44) << "qi::parallel_sort (Multi-Core Radix)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_radix_par
                      << std::setw(24) << (std::to_string(ratio).substr(0, 4) + "x FASTER")
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        std::cout << "----------------------------------------------------------------------------------------\n\n";
    }

    // ── NARROW DOMAIN BENCHMARK (Values 0-255) ──
    {
        size_t N = 1000000;
        std::cout << "--- Dataset Size: N = 1,000,000 Keys (Narrow Domain: Values 0-255) ---\n";
        std::cout << std::left << std::setw(44) << "Algorithm / Engine"
                  << std::setw(16) << "Time (ms)"
                  << std::setw(24) << "vs qi::sort (Baseline)"
                  << "Status\n";
        std::cout << "----------------------------------------------------------------------------------------\n";

        std::vector<uint32_t> original(N);
        std::uniform_int_distribution<uint32_t> dist(0, 255);
        for (size_t i = 0; i < N; ++i) original[i] = dist(rng);

        double t_base = 0;
        {
            auto data = original;
            t_base = time_ms([&]() {
                data = original;
                qi::sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            std::cout << std::left << std::setw(44) << "qi::sort (Production Radix - BASELINE)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_base
                      << std::setw(24) << "1.00x (BASELINE)"
                      << (ok ? "PASS" : "FAIL") << "\n";
        }
        {
            auto data = original;
            double t_hyb = time_ms([&]() {
                data = original;
                qi_hybrid::sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double ratio = t_base / t_hyb;
            std::string label = (ratio >= 1.0) ? (std::to_string(ratio).substr(0, 4) + "x FASTER") 
                                               : (std::to_string(1.0 / ratio).substr(0, 4) + "x slower");
            std::cout << std::left << std::setw(44) << "QI-Hybrid (Single-Core Engine)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t_hyb
                      << std::setw(24) << label
                      << (ok ? "PASS" : "FAIL") << "\n";
        }
        std::cout << "----------------------------------------------------------------------------------------\n\n";
    }

    return 0;
}
