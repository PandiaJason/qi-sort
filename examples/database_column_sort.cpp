/*
===============================================================================
QI-RADIX EXAMPLE 3: COLUMNAR DATABASE QUERY ENGINE INTEGRATION
===============================================================================
Demonstrates sorting columnar surrogate join keys and timestamp data
in zero-copy memory arrays (e.g., DuckDB / Apache Arrow columnar layouts).
===============================================================================
*/

#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include "../include/qi_radix.hpp"

struct ColumnChunk {
    std::string columnName;
    std::vector<uint32_t> data;
};

int main() {
    std::cout << "=== QI-Radix Columnar Database Sort Example ===\n\n";

    size_t rowCount = 500000;
    ColumnChunk user_ids{"user_id_join_key", std::vector<uint32_t>(rowCount)};

    std::mt19937 rng(12345);
    std::uniform_int_distribution<uint32_t> dist(1000, 50000); // Clustered surrogate IDs

    for (size_t i = 0; i < rowCount; ++i) {
        user_ids.data[i] = dist(rng);
    }

    std::cout << "Sorting column chunk '" << user_ids.columnName << "' (" << rowCount << " rows)...\n";

    auto start = std::chrono::high_resolution_clock::now();
    
    // Sort columnar data chunk adaptively
    qi::sort(user_ids.data);

    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "Successfully sorted " << rowCount << " database keys in " << ms << " ms!\n";
    std::cout << "First 5 sorted keys: ";
    for (int i = 0; i < 5; ++i) std::cout << user_ids.data[i] << " ";
    std::cout << "\n";

    return 0;
}
