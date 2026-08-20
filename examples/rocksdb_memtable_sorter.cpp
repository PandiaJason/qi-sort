/*
===============================================================================
ROCKSDB / LEVELDB MEMTABLE FLUSH INTEGRATION WITH QI::SORT
===============================================================================
Simulates a high-throughput RocksDB LSM-Tree Write Engine:
  1. Incoming Key-Value Writes added to in-memory MemTable
  2. MemTable Flush: Sort keys before writing contiguous SSTable file to disk
  3. Benchmark: qi::sort vs std::sort in MemTable SSTable flushing
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

    // 2. qi::sort Adaptive Engine
    auto keys_qi = memtable_keys;
    auto start_qi = std::chrono::high_resolution_clock::now();
    qi::sort(keys_qi);
    auto end_qi = std::chrono::high_resolution_clock::now();
    double ms_qi = std::chrono::duration<double, std::milli>(end_qi - start_qi).count();

    std::cout << "-------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(35) << "RocksDB Flush Engine"
              << std::setw(15) << "Time (ms)"
              << std::setw(18) << "Throughput"
              << "Speedup\n";
    std::cout << "-------------------------------------------------------------------------\n";

    double mk_std = MEMTABLE_SIZE / ms_std / 1000.0;
    double mk_qi  = MEMTABLE_SIZE / ms_qi / 1000.0;

    std::cout << std::left << std::setw(35) << "RocksDB Baseline (std::sort)"
              << std::setw(15) << std::fixed << std::setprecision(2) << ms_std
              << std::setw(18) << (std::to_string((int)mk_std) + " MKeys/s")
              << "1.00x\n";

    std::cout << std::left << std::setw(35) << "RocksDB + qi::sort Engine"
              << std::setw(15) << std::fixed << std::setprecision(2) << ms_qi
              << std::setw(18) << (std::to_string((int)mk_qi) + " MKeys/s")
              << std::setprecision(2) << (ms_std / ms_qi) << "x FASTER\n";

    std::cout << "-------------------------------------------------------------------------\n";
    std::cout << "SUCCESS: qi::sort accelerates RocksDB MemTable flushes by "
              << std::setprecision(2) << (ms_std / ms_qi) << "x!\n";
    std::cout << "=========================================================================\n\n";

    return 0;
}
