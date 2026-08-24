#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include "include/qi_sort_univ.hpp"
#include "include/qi_radix.hpp"

int main() {
    constexpr size_t N = 2'000'000;
    std::vector<uint32_t> data(N);
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFFu);

    for (size_t i = 0; i < N; ++i) data[i] = dist(rng);

    // Test qi_univ correctness
    auto test_data = data;
    qi_univ::sort_univ(test_data.data(), N);
    bool ok = std::is_sorted(test_data.begin(), test_data.end());
    std::cout << "[qi_sort_univ] Correctness check: " << (ok ? "PASS" : "FAIL") << "\n";

    // Benchmark comparison
    double min_univ = 1e9, min_std = 1e9;
    for (int run = 0; run < 5; ++run) {
        auto d1 = data;
        auto t0 = std::chrono::high_resolution_clock::now();
        qi_univ::sort_univ(d1.data(), N);
        auto t1 = std::chrono::high_resolution_clock::now();
        double dt1 = std::chrono::duration<double, std::milli>(t1 - t0).count();
        min_univ = std::min(min_univ, dt1);

        auto d2 = data;
        t0 = std::chrono::high_resolution_clock::now();
        std::sort(d2.begin(), d2.end());
        t1 = std::chrono::high_resolution_clock::now();
        double dt2 = std::chrono::duration<double, std::milli>(t1 - t0).count();
        min_std = std::min(min_std, dt2);
    }

    std::cout << "std::sort:      " << min_std << " ms\n";
    std::cout << "qi_sort_univ:   " << min_univ << " ms  (" << (min_std / min_univ) << "x speedup)\n";

    return 0;
}
