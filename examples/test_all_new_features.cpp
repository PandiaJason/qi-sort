/*
===============================================================================
COMPREHENSIVE VERIFICATION OF NEW ENTERPRISE QI::SORT FEATURES
===============================================================================
Tests:
  1. Signed Integers (int32_t with negative numbers)
  2. Floating-Point Numbers (float with negative values and decimals)
  3. Key-Payload (Tuple) Sorting (Database ORDER BY simulation)
  4. String Key Prefix Radix Sorting (Text/VARCHAR columns)
  5. Multi-Threaded Parallel Execution Engine (qi::parallel_sort)
===============================================================================
*/

#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <cassert>
#include "../include/qi_radix.hpp"

int main() {
    std::cout << "=======================================================================\n";
    std::cout << " VERIFYING ALL NEW ENTERPRISE QI::SORT FEATURES\n";
    std::cout << "=======================================================================\n\n";

    // 1. SIGNED INTEGER SORTING (int32_t)
    std::cout << "[Test 1/5] Signed Integers (int32_t with negative numbers)...\n";
    std::vector<int32_t> signed_data = {-100, 42, -5000, 0, 9999, -1, 15};
    qi::sort(signed_data);
    assert(std::is_sorted(signed_data.begin(), signed_data.end()));
    std::cout << "  --> Passed! Sorted: ";
    for (auto v : signed_data) std::cout << v << " ";
    std::cout << "\n\n";

    // 2. FLOATING-POINT SORTING (float)
    std::cout << "[Test 2/5] Floating-Point Numbers (float with negatives)...\n";
    std::vector<float> float_data = {-3.14f, 2.71f, -100.5f, 0.0f, 99.9f, -0.001f};
    qi::sort(float_data);
    assert(std::is_sorted(float_data.begin(), float_data.end()));
    std::cout << "  --> Passed! Sorted: ";
    for (auto v : float_data) std::cout << v << " ";
    std::cout << "\n\n";

    // 3. KEY-PAYLOAD (TUPLE) SORTING (Database ORDER BY)
    std::cout << "[Test 3/5] Key-Payload Tuple Sorting (key + row_id)...\n";
    std::vector<uint32_t> keys = {40, 10, 30, 20};
    std::vector<uint64_t> row_ids = {101, 102, 103, 104}; // Associated database row IDs
    qi::sort_pairs(keys, row_ids);
    assert(std::is_sorted(keys.begin(), keys.end()));
    assert(row_ids[0] == 102); // Key 10 -> Row 102
    assert(row_ids[1] == 104); // Key 20 -> Row 104
    assert(row_ids[2] == 103); // Key 30 -> Row 103
    assert(row_ids[3] == 101); // Key 40 -> Row 101
    std::cout << "  --> Passed! Key-Payload Tuples co-sorted correctly!\n\n";

    // 4. STRING KEY PREFIX RADIX SORTING (VARCHAR Columns)
    std::cout << "[Test 4/5] String Prefix Radix Sorting (VARCHAR strings)...\n";
    std::vector<std::string> strings = {"banana", "apple", "cherry", "date", "apricot", "blueberry"};
    qi::sort_strings(strings);
    assert(std::is_sorted(strings.begin(), strings.end()));
    std::cout << "  --> Passed! Sorted Strings: ";
    for (const auto& s : strings) std::cout << s << " ";
    std::cout << "\n\n";

    // 5. MULTI-THREADED PARALLEL ENGINE (qi::parallel_sort)
    std::cout << "[Test 5/5] Multi-Threaded Parallel Sort (qi::parallel_sort)...\n";
    std::vector<uint32_t> large_data(200000);
    for (size_t i = 0; i < large_data.size(); ++i) large_data[i] = static_cast<uint32_t>(rand());
    qi::parallel_sort(large_data);
    assert(std::is_sorted(large_data.begin(), large_data.end()));
    std::cout << "  --> Passed! 200,000 items parallel sorted cleanly!\n\n";

    std::cout << "=======================================================================\n";
    std::cout << " ALL 5 NEW ENTERPRISE FEATURES 100% VERIFIED AND PASSED!\n";
    std::cout << "=======================================================================\n";

    return 0;
}
