#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include "../include/qi_apex.hpp"
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
    std::cout << "  OFFICIAL BENCHMARK: qi::apex vs qi::sort vs std::sort\n";
    std::cout << "========================================================================================\n\n";

    std::vector<size_t> sizes = {100000, 1000000, 10000000};
    std::mt19937_64 rng(1337);

    for (size_t N : sizes) {
        std::cout << "--- Dataset Size: N = " << N << " Elements (Uniform Random 32-bit) ---\n";
        std::cout << std::left << std::setw(44) << "Algorithm / Engine"
                  << std::setw(16) << "Time (ms)"
                  << std::setw(24) << "Throughput (MKeys/s)"
                  << "Status\n";
        std::cout << "----------------------------------------------------------------------------------------\n";

        std::vector<uint32_t> original(N);
        for (size_t i = 0; i < N; ++i) original[i] = rng();

        // 1. std::sort
        {
            auto data = original;
            double t = time_ms([&]() {
                data = original;
                std::sort(data.data(), data.data() + N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double mkeys = (N / 1e6) / (t / 1000.0);
            std::cout << std::left << std::setw(44) << "std::sort (C++ Introsort)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t
                      << std::setw(24) << mkeys
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 2. qi::sort (v0.3.61 Baseline)
        {
            auto data = original;
            double t = time_ms([&]() {
                data = original;
                qi::sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double mkeys = (N / 1e6) / (t / 1000.0);
            std::cout << std::left << std::setw(44) << "qi::sort (v0.3.61 Baseline)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t
                      << std::setw(24) << mkeys
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 3. qi::apex (Single-Core)
        {
            auto data = original;
            double t = time_ms([&]() {
                data = original;
                qi::apex::sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double mkeys = (N / 1e6) / (t / 1000.0);
            std::cout << std::left << std::setw(44) << "qi::apex (Single-Core)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t
                      << std::setw(24) << mkeys
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 4. qi::apex (Multi-Core Parallel)
        {
            auto data = original;
            double t = time_ms([&]() {
                data = original;
                qi::apex::parallel_sort(data.data(), N);
            });
            bool ok = std::is_sorted(data.begin(), data.end());
            double mkeys = (N / 1e6) / (t / 1000.0);
            std::cout << std::left << std::setw(44) << "qi::apex (Multi-Core PARALLEL)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t
                      << std::setw(24) << mkeys
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        std::cout << "----------------------------------------------------------------------------------------\n\n";
    }

    // ── NARROW DOMAIN (0-255) ──
    {
        size_t N = 10000000;
        std::cout << "--- Dataset Size: N = 10,000,000 Keys (Duplicates: Values 0-255) ---\n";
        std::vector<uint32_t> original(N);
        std::uniform_int_distribution<uint32_t> dist(0, 255);
        for (size_t i = 0; i < N; ++i) original[i] = dist(rng);

        auto data = original;
        double t = time_ms([&]() {
            data = original;
            qi::apex::sort(data.data(), N);
        });
        bool ok = std::is_sorted(data.begin(), data.end());
        double mkeys = (N / 1e6) / (t / 1000.0);
        std::cout << std::left << std::setw(44) << "qi::apex (Single-Core)"
                  << std::setw(16) << std::fixed << std::setprecision(2) << t
                  << std::setw(24) << mkeys
                  << (ok ? "PASS" : "FAIL") << "\n\n";
    }

    // ── O(N) MONOTONIC SORTED TEST ──
    {
        size_t N = 10000000;
        std::cout << "--- Dataset Size: N = 10,000,000 Keys (Already Sorted - O(N) Test) ---\n";
        std::vector<uint32_t> original(N);
        for (size_t i = 0; i < N; ++i) original[i] = static_cast<uint32_t>(i);

        auto data = original;
        double t = time_ms([&]() {
            data = original;
            qi::apex::sort(data.data(), N);
        });
        bool ok = std::is_sorted(data.begin(), data.end());
        double mkeys = (N / 1e6) / (t / 1000.0);
        std::cout << std::left << std::setw(44) << "qi::apex (Single-Core)"
                  << std::setw(16) << std::fixed << std::setprecision(2) << t
                  << std::setw(24) << mkeys
                  << (ok ? "PASS" : "FAIL") << "\n\n";
    }

    return 0;
}
