#include "include/qi_radix.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <algorithm>

int main() {
    const size_t N = 100'000'000; // 100 MILLION KEYS (400 MB)
    std::cout << "=========================================================================\n";
    std::cout << "  MASSIVE ENTERPRISE SCALE TEST: N = 100,000,000 KEYS (400 MB RAM)\n";
    std::cout << "=========================================================================\n\n";

    std::cout << "Allocating 100 Million uint32_t elements (400 MB memory)...\n";
    std::vector<uint32_t> data(N);

    // ── TEST 1: UNIFORM RANDOM 100M KEYS ──
    std::cout << "\n[1/3] Generating 100 Million Uniform Random 32-bit Keys...\n";
    std::mt19937 rng(42);
    for (size_t i = 0; i < N; ++i) {
        data[i] = rng();
    }

    std::cout << "Running qi::sort on 100 Million Keys...\n";
    auto t0 = std::chrono::high_resolution_clock::now();
    qi::sort(data);
    auto t1 = std::chrono::high_resolution_clock::now();

    double ms1 = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double mkeys_sec1 = (N / 1'000'000.0) / (ms1 / 1000.0);
    bool pass1 = std::is_sorted(data.begin(), data.end());

    std::cout << "-> Time: " << ms1 << " ms (" << (ms1 / 1000.0) << " seconds)\n";
    std::cout << "-> Throughput: " << std::fixed << std::setprecision(2) << mkeys_sec1 << " Million Keys / sec\n";
    std::cout << "-> Correctness: " << (pass1 ? "PASSED (100% Sorted)" : "FAILED") << "\n";

    // ── TEST 2: HEAVY DUPLICATE 100M CATEGORIES ──
    std::cout << "\n[2/3] Generating 100 Million Heavy Duplicate Category IDs (0-255)...\n";
    for (size_t i = 0; i < N; ++i) {
        data[i] = rng() % 256;
    }

    std::cout << "Running qi::sort on 100 Million Duplicate Keys...\n";
    t0 = std::chrono::high_resolution_clock::now();
    qi::sort(data);
    t1 = std::chrono::high_resolution_clock::now();

    double ms2 = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double mkeys_sec2 = (N / 1'000'000.0) / (ms2 / 1000.0);
    bool pass2 = std::is_sorted(data.begin(), data.end());

    std::cout << "-> Time: " << ms2 << " ms (" << (ms2 / 1000.0) << " seconds)\n";
    std::cout << "-> Throughput: " << std::fixed << std::setprecision(2) << mkeys_sec2 << " Million Keys / sec\n";
    std::cout << "-> Correctness: " << (pass2 ? "PASSED (100% Sorted)" : "FAILED") << "\n";

    // ── TEST 3: MULTI-THREADED PARALLEL 100M SORT ──
    std::cout << "\n[3/3] Generating 100 Million Hash Keys for Parallel Multi-Threaded Sort...\n";
    for (size_t i = 0; i < N; ++i) {
        uint32_t x = rng();
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = (x >> 16) ^ x;
        data[i] = x;
    }

    std::cout << "Running qi::sort_parallel on 100 Million Keys...\n";
    t0 = std::chrono::high_resolution_clock::now();
    qi::sort_parallel(data);
    t1 = std::chrono::high_resolution_clock::now();

    double ms3 = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double mkeys_sec3 = (N / 1'000'000.0) / (ms3 / 1000.0);
    bool pass3 = std::is_sorted(data.begin(), data.end());

    std::cout << "-> Time: " << ms3 << " ms (" << (ms3 / 1000.0) << " seconds)\n";
    std::cout << "-> Throughput: " << std::fixed << std::setprecision(2) << mkeys_sec3 << " Million Keys / sec\n";
    std::cout << "-> Correctness: " << (pass3 ? "PASSED (100% Sorted)" : "FAILED") << "\n";

    std::cout << "\n=========================================================================\n";
    if (pass1 && pass2 && pass3) {
        std::cout << "  SUCCESS: 100 MILLION ROW ENTERPRISE STRESS TEST PASSED 100%!\n";
    } else {
        std::cout << "  FAIL: STRESS TEST DETECTED AN ERROR\n";
        return 1;
    }
    std::cout << "=========================================================================\n";

    return 0;
}
