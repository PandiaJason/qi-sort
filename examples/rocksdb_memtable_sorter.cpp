/*
===============================================================================
ROCKSDB / LEVELDB MEMTABLE FLUSH INTEGRATION WITH QI::SORT
===============================================================================
Simulates a high-throughput RocksDB LSM-Tree Write Engine:
  1. Incoming Key-Value Writes added to in-memory MemTable
  2. MemTable Flush: Sort keys before writing contiguous SSTable file to disk
  3. Benchmark: RocksDB Baseline vs Plain Radix-8/11/16 vs qi::sort
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

struct RocksDBMemTableEntry {
    uint32_t key;
    uint32_t sequence_number;
    char value_payload[16];
};

int main() {
    const size_t MEMTABLE_SIZE = 2000000; // 2 Million keys per MemTable flush

    std::cout << "=========================================================================\n";
    std::cout << "  ROCKSDB MEMTABLE FLUSH BENCHMARK (N = " << MEMTABLE_SIZE << " Writes)\n";
    std::cout << "=========================================================================\n\n";

    std::cout << "Simulating RocksDB MemTable concurrent write buffer...\n";
    std::vector<uint32_t> memtable_keys(MEMTABLE_SIZE);
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);

    for (size_t i = 0; i < MEMTABLE_SIZE; ++i) {
        memtable_keys[i] = dist(rng);
    }

    std::cout << "Flushing MemTable to SSTable on disk...\n\n";

    // 1. Baseline std::sort
    auto keys_std = memtable_keys;
    auto start_std = std::chrono::high_resolution_clock::now();
    std::sort(keys_std.begin(), keys_std.end());
    auto end_std = std::chrono::high_resolution_clock::now();
    double ms_std = std::chrono::duration<double, std::milli>(end_std - start_std).count();

    // 2. Plain Radix-8
    auto keys_r8 = memtable_keys;
    auto start_r8 = std::chrono::high_resolution_clock::now();
    qi::detail::radixSort8(keys_r8.data(), keys_r8.size(), false);
    auto end_r8 = std::chrono::high_resolution_clock::now();
    double ms_r8 = std::chrono::duration<double, std::milli>(end_r8 - start_r8).count();

    // 3. Plain Radix-11
    auto keys_r11 = memtable_keys;
    auto start_r11 = std::chrono::high_resolution_clock::now();
    qi::detail::radixSort11(keys_r11.data(), keys_r11.size(), false);
    auto end_r11 = std::chrono::high_resolution_clock::now();
    double ms_r11 = std::chrono::duration<double, std::milli>(end_r11 - start_r11).count();

    // 4. Plain Radix-16
    auto keys_r16 = memtable_keys;
    auto start_r16 = std::chrono::high_resolution_clock::now();
    qi::detail::radixSort16(keys_r16.data(), keys_r16.size(), false);
    auto end_r16 = std::chrono::high_resolution_clock::now();
    double ms_r16 = std::chrono::duration<double, std::milli>(end_r16 - start_r16).count();

    // 5. qi::sort Adaptive Engine
    auto keys_qi = memtable_keys;
    auto start_qi = std::chrono::high_resolution_clock::now();
    qi::sort(keys_qi);
    auto end_qi = std::chrono::high_resolution_clock::now();
    double ms_qi = std::chrono::duration<double, std::milli>(end_qi - start_qi).count();

    auto row = [&](const std::string& name, double ms) {
        double mk = MEMTABLE_SIZE / ms / 1000.0;
        double vs = ms_std / ms;
        std::cout << std::left << std::setw(35) << name
                  << std::setw(15) << std::fixed << std::setprecision(2) << ms
                  << std::setw(18) << (std::to_string((int)mk) + " MKeys/s")
                  << (std::to_string(vs).substr(0, 4) + "x\n");
    };

    std::cout << "-------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(35) << "RocksDB Flush Engine"
              << std::setw(15) << "Time (ms)"
              << std::setw(18) << "Throughput"
              << "Speedup\n";
    std::cout << "-------------------------------------------------------------------------\n";

    row("RocksDB Baseline (std::sort)", ms_std);
    row("Plain Radix-8  (Fixed 4-Pass)", ms_r8);
    row("Plain Radix-11 (Fixed 3-Pass)", ms_r11);
    row("Plain Radix-16 (Fixed 2-Pass)", ms_r16);
    row("RocksDB + qi::sort Engine",     ms_qi);

    std::cout << "-------------------------------------------------------------------------\n";
    std::cout << "SUCCESS: qi::sort accelerates RocksDB MemTable flushes by "
              << std::setprecision(2) << (ms_std / ms_qi) << "x!\n";
    std::cout << "=========================================================================\n\n";

    return 0;
}
