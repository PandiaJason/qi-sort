#include "include/qi_radix.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <algorithm>

int main() {
    const size_t N = 5'000'000; // 5 MILLION KEYS
    std::cout << "=========================================================================\n";
    std::cout << "  APPLES-TO-APPLES RADIX KERNEL COMPARISON (N = " << N << " uint32_t Keys)\n";
    std::cout << "=========================================================================\n\n";

    std::vector<uint32_t> orig(N);
    std::mt19937 rng(42);
    for (size_t i = 0; i < N; ++i) orig[i] = rng();

    // 1. Plain Radix-8 (Fixed 4 Passes)
    auto d8 = orig;
    auto t0 = std::chrono::high_resolution_clock::now();
    qi::detail::radixSort8(d8.data(), d8.size());
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms8 = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // 2. Plain Radix-11 (Fixed 3 Passes)
    auto d11 = orig;
    t0 = std::chrono::high_resolution_clock::now();
    qi::detail::radixSort11(d11.data(), d11.size());
    t1 = std::chrono::high_resolution_clock::now();
    double ms11 = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // 3. Plain Radix-16 (Fixed 2 Passes)
    auto d16 = orig;
    t0 = std::chrono::high_resolution_clock::now();
    qi::detail::radixSort16(d16.data(), d16.size());
    t1 = std::chrono::high_resolution_clock::now();
    double ms16 = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // 4. qi::sort (Quick Index Engine)
    auto dqi = orig;
    t0 = std::chrono::high_resolution_clock::now();
    qi::sort(dqi);
    t1 = std::chrono::high_resolution_clock::now();
    double msqi = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << std::left << std::setw(30) << "Kernel"
              << std::setw(20) << "Passes"
              << std::setw(20) << "Execution Time (ms)" << "\n";
    std::cout << std::string(70, '-') << "\n";

    std::cout << std::left << std::setw(30) << "Plain Radix-8"
              << std::setw(20) << "4 Passes"
              << (std::to_string(ms8).substr(0, 5) + " ms\n");

    std::cout << std::left << std::setw(30) << "Plain Radix-11"
              << std::setw(20) << "3 Passes"
              << (std::to_string(ms11).substr(0, 5) + " ms\n");

    std::cout << std::left << std::setw(30) << "Plain Radix-16"
              << std::setw(20) << "2 Passes"
              << (std::to_string(ms16).substr(0, 5) + " ms\n");

    std::cout << std::left << std::setw(30) << "qi::sort (Quick Index Engine)"
              << std::setw(20) << "2 Passes (Adaptive)"
              << (std::to_string(msqi).substr(0, 5) + " ms\n");

    std::cout << "\n=========================================================================\n";
    return 0;
}
