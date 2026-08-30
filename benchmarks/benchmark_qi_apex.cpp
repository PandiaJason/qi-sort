#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <cstring>
#include "../include/qi_apex.hpp"
#include "../include/qi_radix.hpp"

// ── Standard Textbook 4-Pass LSD Radix-8 (No adaptive sensing, fixed 4 passes) ──
void standard_radix8(uint32_t* data, size_t n) {
    if (n <= 1) return;
    std::vector<uint32_t> buffer(n);
    uint32_t* src = data;
    uint32_t* dst = buffer.data();

    for (int pass = 0; pass < 4; ++pass) {
        int shift = pass * 8;
        uint32_t count[256] = {};
        for (size_t i = 0; i < n; ++i) count[(src[i] >> shift) & 0xFF]++;

        uint32_t sum = 0;
        for (int k = 0; k < 256; ++k) {
            uint32_t c = count[k]; count[k] = sum; sum += c;
        }

        for (size_t i = 0; i < n; ++i) {
            uint32_t v = src[i];
            dst[count[(v >> shift) & 0xFF]++] = v;
        }
        std::swap(src, dst);
    }
    if (src != data) std::memcpy(data, src, n * sizeof(uint32_t));
}

// ── Standard Textbook 2-Pass LSD Radix-16 (No adaptive sensing, fixed 2 passes) ──
void standard_radix16(uint32_t* data, size_t n) {
    if (n <= 1) return;
    std::vector<uint32_t> buffer(n);
    uint32_t* src = data;
    uint32_t* dst = buffer.data();

    for (int pass = 0; pass < 2; ++pass) {
        int shift = pass * 16;
        std::vector<uint32_t> count(65536, 0);
        for (size_t i = 0; i < n; ++i) count[(src[i] >> shift) & 0xFFFF]++;

        uint32_t sum = 0;
        for (int k = 0; k < 65536; ++k) {
            uint32_t c = count[k]; count[k] = sum; sum += c;
        }

        for (size_t i = 0; i < n; ++i) {
            uint32_t v = src[i];
            dst[count[(v >> shift) & 0xFFFF]++] = v;
        }
        std::swap(src, dst);
    }
    if (src != data) std::memcpy(data, src, n * sizeof(uint32_t));
}

template <typename Func>
double time_ms(Func f, int iterations = 3) {
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
    std::cout << "  OFFICIAL BENCHMARK: qi::apex vs Standard Radix vs qi::sort vs std::sort\n";
    std::cout << "  Hardware: Apple Silicon M1 Pro | clang++ -O3 -std=c++17 | Best of 3 Runs\n";
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
            double t = time_ms([&]() { data = original; std::sort(data.data(), data.data() + N); });
            bool ok = std::is_sorted(data.begin(), data.end());
            double mkeys = (N / 1e6) / (t / 1000.0);
            std::cout << std::left << std::setw(44) << "std::sort (C++ Introsort)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t
                      << std::setw(24) << mkeys
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 2. Standard 4-Pass Radix-8
        {
            auto data = original;
            double t = time_ms([&]() { data = original; standard_radix8(data.data(), N); });
            bool ok = std::is_sorted(data.begin(), data.end());
            double mkeys = (N / 1e6) / (t / 1000.0);
            std::cout << std::left << std::setw(44) << "Standard Radix-8 (4 Passes)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t
                      << std::setw(24) << mkeys
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 3. Standard 2-Pass Radix-16
        {
            auto data = original;
            double t = time_ms([&]() { data = original; standard_radix16(data.data(), N); });
            bool ok = std::is_sorted(data.begin(), data.end());
            double mkeys = (N / 1e6) / (t / 1000.0);
            std::cout << std::left << std::setw(44) << "Standard Radix-16 (2 Passes)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t
                      << std::setw(24) << mkeys
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 4. qi::sort (v0.3.61 Baseline)
        {
            auto data = original;
            double t = time_ms([&]() { data = original; qi::sort(data.data(), N); });
            bool ok = std::is_sorted(data.begin(), data.end());
            double mkeys = (N / 1e6) / (t / 1000.0);
            std::cout << std::left << std::setw(44) << "qi::sort (v0.3.61 Baseline)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t
                      << std::setw(24) << mkeys
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 5. qi::apex (Single-Core Champion)
        {
            auto data = original;
            double t = time_ms([&]() { data = original; qi::apex::sort(data.data(), N); });
            bool ok = std::is_sorted(data.begin(), data.end());
            double mkeys = (N / 1e6) / (t / 1000.0);
            std::cout << std::left << std::setw(44) << "qi::apex (Single-Core Champion)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t
                      << std::setw(24) << mkeys
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // 6. qi::apex (Multi-Core Parallel)
        {
            auto data = original;
            double t = time_ms([&]() { data = original; qi::apex::parallel_sort(data.data(), N); });
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
        std::cout << std::left << std::setw(44) << "Algorithm / Engine"
                  << std::setw(16) << "Time (ms)"
                  << std::setw(24) << "Throughput (MKeys/s)"
                  << "Status\n";
        std::cout << "----------------------------------------------------------------------------------------\n";
        std::vector<uint32_t> original(N);
        std::uniform_int_distribution<uint32_t> dist(0, 255);
        for (size_t i = 0; i < N; ++i) original[i] = dist(rng);

        // Standard Radix-8
        {
            auto data = original;
            double t = time_ms([&]() { data = original; standard_radix8(data.data(), N); });
            bool ok = std::is_sorted(data.begin(), data.end());
            double mkeys = (N / 1e6) / (t / 1000.0);
            std::cout << std::left << std::setw(44) << "Standard Radix-8 (4 Passes)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t
                      << std::setw(24) << mkeys
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // qi::apex
        {
            auto data = original;
            double t = time_ms([&]() { data = original; qi::apex::sort(data.data(), N); });
            bool ok = std::is_sorted(data.begin(), data.end());
            double mkeys = (N / 1e6) / (t / 1000.0);
            std::cout << std::left << std::setw(44) << "qi::apex (Adaptive Counting Sort)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t
                      << std::setw(24) << mkeys
                      << (ok ? "PASS" : "FAIL") << "\n\n";
        }
    }

    // ── O(N) MONOTONIC SORTED TEST ──
    {
        size_t N = 10000000;
        std::cout << "--- Dataset Size: N = 10,000,000 Keys (Already Sorted - O(N) Test) ---\n";
        std::cout << std::left << std::setw(44) << "Algorithm / Engine"
                  << std::setw(16) << "Time (ms)"
                  << std::setw(24) << "Throughput (MKeys/s)"
                  << "Status\n";
        std::cout << "----------------------------------------------------------------------------------------\n";
        std::vector<uint32_t> original(N);
        for (size_t i = 0; i < N; ++i) original[i] = static_cast<uint32_t>(i);

        // Standard Radix-8
        {
            auto data = original;
            double t = time_ms([&]() { data = original; standard_radix8(data.data(), N); });
            bool ok = std::is_sorted(data.begin(), data.end());
            double mkeys = (N / 1e6) / (t / 1000.0);
            std::cout << std::left << std::setw(44) << "Standard Radix-8 (4 Passes)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t
                      << std::setw(24) << mkeys
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        // qi::apex
        {
            auto data = original;
            double t = time_ms([&]() { data = original; qi::apex::sort(data.data(), N); });
            bool ok = std::is_sorted(data.begin(), data.end());
            double mkeys = (N / 1e6) / (t / 1000.0);
            std::cout << std::left << std::setw(44) << "qi::apex (1ns Monotonic Short-Circuit)"
                      << std::setw(16) << std::fixed << std::setprecision(2) << t
                      << std::setw(24) << mkeys
                      << (ok ? "PASS" : "FAIL") << "\n\n";
        }
    }

    return 0;
}
