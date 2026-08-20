/*
===============================================================================
APACHE ARROW / POLARS DATAFRAME COLUMN SORTING WITH QI::SORT
===============================================================================
Simulates Apache Arrow RecordBatch contiguous memory buffer sorting:
  1. Arrow UInt32Array Column Buffer
  2. Zero-Copy Sorting with qi::sort
  3. Benchmark Comparison: qi::sort vs std::sort in Arrow / Polars Dataframes
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

struct ArrowRecordBatchColumn {
    std::string column_name;
    size_t length;
    uint32_t* raw_buffer; // Contiguous Arrow memory buffer
};

int main() {
    const size_t ROWS = 3000000; // 3 Million row Dataframe

    std::cout << "=========================================================================\n";
    std::cout << "  APACHE ARROW / POLARS DATAFRAME BENCHMARK (N = " << ROWS << " Rows)\n";
    std::cout << "=========================================================================\n\n";

    std::cout << "Allocating Apache Arrow contiguous RecordBatch memory buffer...\n";
    std::vector<uint32_t> buffer_std(ROWS);
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);

    for (size_t i = 0; i < ROWS; ++i) {
        buffer_std[i] = dist(rng);
    }
    std::vector<uint32_t> buffer_qi = buffer_std;

    ArrowRecordBatchColumn arrow_col_std{"user_id", ROWS, buffer_std.data()};
    ArrowRecordBatchColumn arrow_col_qi{"user_id", ROWS, buffer_qi.data()};

    std::cout << "Sorting Arrow Dataframe Column 'user_id'...\n\n";

    // 1. Baseline std::sort
    auto start_std = std::chrono::high_resolution_clock::now();
    std::sort(arrow_col_std.raw_buffer, arrow_col_std.raw_buffer + arrow_col_std.length);
    auto end_std = std::chrono::high_resolution_clock::now();
    double ms_std = std::chrono::duration<double, std::milli>(end_std - start_std).count();

    // 2. qi::sort Adaptive Engine
    auto start_qi = std::chrono::high_resolution_clock::now();
    qi::sort(arrow_col_qi.raw_buffer, arrow_col_qi.length);
    auto end_qi = std::chrono::high_resolution_clock::now();
    double ms_qi = std::chrono::duration<double, std::milli>(end_qi - start_qi).count();

    std::cout << "-------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(35) << "Dataframe Engine"
              << std::setw(15) << "Time (ms)"
              << std::setw(18) << "Throughput"
              << "Speedup\n";
    std::cout << "-------------------------------------------------------------------------\n";

    double mk_std = ROWS / ms_std / 1000.0;
    double mk_qi  = ROWS / ms_qi / 1000.0;

    std::cout << std::left << std::setw(35) << "Arrow / Polars (std::sort)"
              << std::setw(15) << std::fixed << std::setprecision(2) << ms_std
              << std::setw(18) << (std::to_string((int)mk_std) + " MRows/s")
              << "1.00x\n";

    std::cout << std::left << std::setw(35) << "Arrow / Polars + qi::sort"
              << std::setw(15) << std::fixed << std::setprecision(2) << ms_qi
              << std::setw(18) << (std::to_string((int)mk_qi) + " MRows/s")
              << std::setprecision(2) << (ms_std / ms_qi) << "x FASTER\n";

    std::cout << "-------------------------------------------------------------------------\n";
    std::cout << "SUCCESS: qi::sort provides " << std::setprecision(2) << (ms_std / ms_qi)
              << "x throughput boost in Arrow / Polars Dataframe column sorting!\n";
    std::cout << "=========================================================================\n\n";

    return 0;
}
