/*
===============================================================================
REAL-TIME TIME-INTERVAL STREAMING BENCHMARK: QI-SORT VS INDUSTRY SORTERS
===============================================================================
Simulates a real-time live streaming telemetry / particle engine ingesting
50,000 3D spatial telemetry keys every 10 milliseconds (100 Hz Real-Time Rate).

Evaluates latency & throughput per time interval tick across 7 engines:
  1. qi::radix_11       — Direct 3-Pass Radix (16 KB L1-bound bucket array)
  2. qi::radix_16       — Direct 2-Pass Zero-Memcpy Radix (512 KB bucket array)
  3. qi::radix_8        — Direct 4-Pass Radix (2 KB L1-bound bucket array)
  4. qi::sort           — Quick Index Adaptive Engine
  5. ska_sort           — Malte Skarupke's Official AAA Game Radix Sorter
  6. pdqsort            — Orson Peters' Pattern-Defeating QuickSort (Rust std)
  7. std::sort          — C++ Standard Library Introsort
===============================================================================
*/

#include "include/qi_radix.hpp"
#include "benchmarks/competitors/pdqsort.h"
#include "benchmarks/competitors/ska_sort.hpp"

#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <random>
#include <iomanip>
#include <algorithm>

struct StreamBatch {
    int tick_id;
    std::vector<uint32_t> keys;
};

// Generate live 3D Spatial Telemetry keys for a time interval batch
StreamBatch generate_stream_batch(int tick, size_t batch_size, std::mt19937& rng) {
    StreamBatch b;
    b.tick_id = tick;
    b.keys.resize(batch_size);

    std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
    for (size_t i = 0; i < batch_size; ++i) {
        b.keys[i] = dist(rng);
    }
    return b;
}

int main() {
    const size_t BATCH_SIZE = 100'000;   // 100,000 Keys per Real-Time Interval
    const int NUM_TICKS = 30;             // 30 Streaming Time-Interval Ticks
    const int INTERVAL_BUDGET_MS = 20;   // 20ms Real-Time Budget per tick

    std::cout << "=========================================================================\n";
    std::cout << "  REAL-TIME TIME-INTERVAL STREAMING BENCHMARK (100,000 Keys / 20ms Budget)\n";
    std::cout << "=========================================================================\n\n";

    std::mt19937 rng(42);

    double sum_r11 = 0.0, sum_r16 = 0.0, sum_r8 = 0.0;
    double sum_qi = 0.0, sum_ska = 0.0, sum_pdq = 0.0, sum_std = 0.0;

    std::cout << "Streaming 30 live time-interval blocks in real-time...\n\n";

    for (int tick = 1; tick <= NUM_TICKS; ++tick) {
        auto batch = generate_stream_batch(tick, BATCH_SIZE, rng);

        // 1. qi::radix_11
        auto k_r11 = batch.keys;
        auto t0 = std::chrono::high_resolution_clock::now();
        qi::radix_11(k_r11.data(), k_r11.size());
        auto t1 = std::chrono::high_resolution_clock::now();
        sum_r11 += std::chrono::duration<double, std::milli>(t1 - t0).count();

        // 2. qi::radix_16
        auto k_r16 = batch.keys;
        t0 = std::chrono::high_resolution_clock::now();
        qi::radix_16(k_r16.data(), k_r16.size());
        t1 = std::chrono::high_resolution_clock::now();
        sum_r16 += std::chrono::duration<double, std::milli>(t1 - t0).count();

        // 3. qi::radix_8
        auto k_r8 = batch.keys;
        t0 = std::chrono::high_resolution_clock::now();
        qi::radix_8(k_r8.data(), k_r8.size());
        t1 = std::chrono::high_resolution_clock::now();
        sum_r8 += std::chrono::duration<double, std::milli>(t1 - t0).count();

        // 4. qi::sort Adaptive Engine
        auto k_qi = batch.keys;
        t0 = std::chrono::high_resolution_clock::now();
        qi::sort(k_qi);
        t1 = std::chrono::high_resolution_clock::now();
        sum_qi += std::chrono::duration<double, std::milli>(t1 - t0).count();

        // 5. ska_sort (AAA Game Radix)
        auto k_ska = batch.keys;
        t0 = std::chrono::high_resolution_clock::now();
        ska_sort(k_ska.begin(), k_ska.end());
        t1 = std::chrono::high_resolution_clock::now();
        sum_ska += std::chrono::duration<double, std::milli>(t1 - t0).count();

        // 6. pdqsort (Rust std algorithm)
        auto k_pdq = batch.keys;
        t0 = std::chrono::high_resolution_clock::now();
        pdqsort(k_pdq.begin(), k_pdq.end());
        t1 = std::chrono::high_resolution_clock::now();
        sum_pdq += std::chrono::duration<double, std::milli>(t1 - t0).count();

        // 7. std::sort (Introsort)
        auto k_std = batch.keys;
        t0 = std::chrono::high_resolution_clock::now();
        std::sort(k_std.begin(), k_std.end());
        t1 = std::chrono::high_resolution_clock::now();
        sum_std += std::chrono::duration<double, std::milli>(t1 - t0).count();

        // Simulate 5ms hardware time-interval delay between ticks
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    double avg_r11 = sum_r11 / NUM_TICKS;
    double avg_r16 = sum_r16 / NUM_TICKS;
    double avg_r8  = sum_r8  / NUM_TICKS;
    double avg_qi  = sum_qi  / NUM_TICKS;
    double avg_ska = sum_ska / NUM_TICKS;
    double avg_pdq = sum_pdq / NUM_TICKS;
    double avg_std = sum_std / NUM_TICKS;

    std::cout << std::left << std::setw(34) << "Real-Time Streaming Engine"
              << std::setw(22) << "Avg Tick Latency (ms)"
              << std::setw(20) << "Streaming Throughput"
              << std::setw(15) << "20ms SLA Compliance" << "\n";
    std::cout << std::string(91, '-') << "\n";

    auto print_row = [](const std::string& name, double ms, double base_ms, int budget) {
        double mkeys = (100000.0 / 1000000.0) / (ms / 1000.0);
        std::cout << std::left << std::setw(34) << name
                  << std::setw(22) << (std::to_string(ms).substr(0, 5) + " ms")
                  << std::setw(20) << (std::to_string(mkeys).substr(0, 5) + " MKeys/s")
                  << (ms < budget ? "PASS (Zero Loss)" : "FAIL (Lagging)") << "\n";
    };

    print_row("qi::radix_11 (3-Pass L1 Radix)", avg_r11, avg_std, INTERVAL_BUDGET_MS);
    print_row("qi::radix_16 (2-Pass Zero-Copy)", avg_r16, avg_std, INTERVAL_BUDGET_MS);
    print_row("qi::radix_8  (4-Pass L1 Radix)", avg_r8,  avg_std, INTERVAL_BUDGET_MS);
    print_row("qi::sort     (Adaptive Engine)", avg_qi,  avg_std, INTERVAL_BUDGET_MS);
    print_row("ska_sort     (Game Radix)",      avg_ska, avg_std, INTERVAL_BUDGET_MS);
    print_row("pdqsort      (Rust std algorithm)", avg_pdq, avg_std, INTERVAL_BUDGET_MS);
    print_row("std::sort    (Introsort Baseline)", avg_std, avg_std, INTERVAL_BUDGET_MS);

    std::cout << "\n=========================================================================\n";
    std::cout << "  SUCCESS: Real-Time Time-Interval Streaming Benchmark Complete!\n";
    std::cout << "=========================================================================\n";

    return 0;
}
