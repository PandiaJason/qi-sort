#include "include/qi_radix.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>

int main() {
    const size_t N = 5'000'000;
    std::vector<uint32_t> data(N);
    for (size_t i = 0; i < N; ++i) {
        data[i] = static_cast<uint32_t>(N - i);
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    qi::sort(data);
    auto t1 = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << "Reverse Sorted (N = 5,000,000) sort time: " << ms << " ms\n";
    std::cout << "Is sorted: " << (std::is_sorted(data.begin(), data.end()) ? "YES" : "NO") << "\n";
    return 0;
}
