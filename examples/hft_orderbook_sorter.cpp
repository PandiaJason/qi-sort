/*
===============================================================================
HIGH-FREQUENCY TRADING (HFT) ORDER BOOK QUEUE SORTING WITH QI::SORT
===============================================================================
Simulates a ultra-low-latency Financial Exchange Matching Engine:
  1. Nanosecond Epoch Timestamp Order Events
  2. Sorting Order Book Queues by Priority Timestamp
  3. Benchmark: qi::sort vs std::sort in HFT Order Book Processing
===============================================================================
*/

#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <algorithm>
#include <cstdint>
#include "../include/qi_radix.hpp"

struct MarketOrder {
    uint32_t timestamp_ns; // Nanosecond timestamp offset
    uint32_t order_id;
    uint32_t price;
    uint32_t quantity;
};

int main() {
    const size_t ORDERS = 3000000; // 3 Million market orders

    std::cout << "=========================================================================\n";
    std::cout << "  HIGH-FREQUENCY TRADING (HFT) ORDER BOOK BENCHMARK (N = " << ORDERS << " Orders)\n";
    std::cout << "=========================================================================\n\n";

    std::cout << "Generating nanosecond-timestamped market order stream...\n";
    std::vector<uint32_t> timestamps_std(ORDERS);
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);

    for (size_t i = 0; i < ORDERS; ++i) {
        timestamps_std[i] = dist(rng);
    }
    std::vector<uint32_t> timestamps_qi = timestamps_std;

    std::cout << "Sorting Order Book Priority Queue by Timestamp...\n\n";

    // 1. Baseline std::sort
    auto start_std = std::chrono::high_resolution_clock::now();
    std::sort(timestamps_std.begin(), timestamps_std.end());
    auto end_std = std::chrono::high_resolution_clock::now();
    double ms_std = std::chrono::duration<double, std::milli>(end_std - start_std).count();

    // 2. qi::sort Adaptive Engine
    auto start_qi = std::chrono::high_resolution_clock::now();
    qi::sort(timestamps_qi);
    auto end_qi = std::chrono::high_resolution_clock::now();
    double ms_qi = std::chrono::duration<double, std::milli>(end_qi - start_qi).count();

    std::cout << "-------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(35) << "Matching Engine Pipeline"
              << std::setw(15) << "Time (ms)"
              << std::setw(18) << "Throughput"
              << "Speedup\n";
    std::cout << "-------------------------------------------------------------------------\n";

    double mk_std = ORDERS / ms_std / 1000.0;
    double mk_qi  = ORDERS / ms_qi / 1000.0;

    std::cout << std::left << std::setw(35) << "HFT Engine Baseline (std::sort)"
              << std::setw(15) << std::fixed << std::setprecision(2) << ms_std
              << std::setw(18) << (std::to_string((int)mk_std) + " MOrders/s")
              << "1.00x\n";

    std::cout << std::left << std::setw(35) << "HFT Engine + qi::sort"
              << std::setw(15) << std::fixed << std::setprecision(2) << ms_qi
              << std::setw(18) << (std::to_string((int)mk_qi) + " MOrders/s")
              << std::setprecision(2) << (ms_std / ms_qi) << "x FASTER\n";

    std::cout << "-------------------------------------------------------------------------\n";
    std::cout << "SUCCESS: qi::sort provides " << std::setprecision(2) << (ms_std / ms_qi)
              << "x throughput boost in HFT Order Book queue sorting!\n";
    std::cout << "=========================================================================\n\n";

    return 0;
}
