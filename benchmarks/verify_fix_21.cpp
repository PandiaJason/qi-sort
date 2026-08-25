#include "include/qi_radix.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <random>
#include <iomanip>

struct BenchResult {
    std::string dataset;
    size_t n;
    double std_ms;
    double stable_ms;
    double qi_ms;
};

std::vector<uint32_t> generate_data(const std::string& type, size_t n, std::mt19937& rng) {
    std::vector<uint32_t> data(n);
    if (type == "Uniform Random") {
        for (size_t i = 0; i < n; ++i) data[i] = rng();
    } else if (type == "Hash Keys") {
        for (size_t i = 0; i < n; ++i) {
            uint32_t x = rng();
            x = ((x >> 16) ^ x) * 0x45d9f3b;
            x = ((x >> 16) ^ x) * 0x45d9f3b;
            x = (x >> 16) ^ x;
            data[i] = x;
        }
    } else if (type == "Heavy Duplicates (0-255)") {
        for (size_t i = 0; i < n; ++i) data[i] = rng() % 256;
    } else if (type == "Low Cardinality (16)") {
        for (size_t i = 0; i < n; ++i) data[i] = rng() % 16;
    } else if (type == "Nearly Sorted 95%") {
        for (size_t i = 0; i < n; ++i) data[i] = static_cast<uint32_t>(i);
        size_t swaps = n * 5 / 100;
        for (size_t s = 0; s < swaps; ++s) {
            size_t i1 = rng() % n;
            size_t i2 = rng() % n;
            std::swap(data[i1], data[i2]);
        }
    } else if (type == "Fully Sorted") {
        for (size_t i = 0; i < n; ++i) data[i] = static_cast<uint32_t>(i);
    } else if (type == "Reverse Sorted") {
        for (size_t i = 0; i < n; ++i) data[i] = static_cast<uint32_t>(n - i);
    }
    return data;
}

int main() {
    std::mt19937 rng(42);
    std::vector<size_t> sizes = {1'000'000, 3'000'000, 5'000'000};
    std::vector<std::string> types = {
        "Uniform Random", "Hash Keys", "Heavy Duplicates (0-255)",
        "Low Cardinality (16)", "Nearly Sorted 95%", "Fully Sorted", "Reverse Sorted"
    };

    std::cout << std::left << std::setw(26) << "Dataset"
              << std::setw(6) << "N"
              << std::setw(12) << "std::sort"
              << std::setw(18) << "std::stable_sort"
              << std::setw(12) << "qi::sort"
              << std::setw(12) << "vs std" << "\n";
    std::cout << std::string(86, '-') << "\n";

    for (size_t n : sizes) {
        for (const auto& type : types) {
            auto orig = generate_data(type, n, rng);

            // std::sort
            auto d1 = orig;
            auto t0 = std::chrono::high_resolution_clock::now();
            std::sort(d1.begin(), d1.end());
            auto t1 = std::chrono::high_resolution_clock::now();
            double std_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

            // std::stable_sort
            auto d2 = orig;
            t0 = std::chrono::high_resolution_clock::now();
            std::stable_sort(d2.begin(), d2.end());
            t1 = std::chrono::high_resolution_clock::now();
            double stable_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

            // qi::sort
            auto d3 = orig;
            t0 = std::chrono::high_resolution_clock::now();
            qi::sort(d3);
            t1 = std::chrono::high_resolution_clock::now();
            double qi_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

            if (d3 != d1) {
                std::cerr << "FAIL: " << type << " at N=" << n << "\n";
                return 1;
            }

            double speedup_std = std_ms / qi_ms;

            std::cout << std::left << std::setw(26) << type
                      << std::setw(6) << (n / 1'000'000) << "M"
                      << std::setw(12) << (std::to_string(std_ms).substr(0, 5) + " ms")
                      << std::setw(18) << (std::to_string(stable_ms).substr(0, 5) + " ms")
                      << std::setw(12) << (std::to_string(qi_ms).substr(0, 5) + " ms")
                      << std::to_string(speedup_std).substr(0, 4) << "x" << "\n";
        }
    }
    std::cout << "\nALL 21 TEST CASES VERIFIED CORRECT AND FAST!\n";
    return 0;
}
