/*
===============================================================================
QI-RADIX EXAMPLE 1: BASIC USAGE
===============================================================================
Demonstrates sorting std::vector, C-style arrays, and iterator ranges.
===============================================================================
*/

#include <iostream>
#include <vector>
#include "../include/qi_radix.hpp"

int main() {
    std::cout << "=== QI-Radix Basic Usage Example ===\n\n";

    // Example 1: Sorting a std::vector<uint32_t>
    std::vector<uint32_t> numbers = {10543, 42, 999999, 12, 0, 42, 8881, 100};
    
    std::cout << "Original vector : ";
    for (uint32_t x : numbers) std::cout << x << " ";
    std::cout << "\n";

    // Call qi::sort directly on vector
    qi::sort(numbers);

    std::cout << "Sorted vector   : ";
    for (uint32_t x : numbers) std::cout << x << " ";
    std::cout << "\n\n";

    // Example 2: Sorting a raw C-style array with options
    uint32_t rawArray[] = {500, 20, 100, 5, 200, 10};
    size_t arraySize = sizeof(rawArray) / sizeof(rawArray[0]);

    qi::SortOptions options;
    options.verbose = true; // Telemetry enabled

    std::cout << "Sorting raw array with verbose options:\n";
    qi::sort(rawArray, arraySize, options);

    std::cout << "Sorted raw array: ";
    for (size_t i = 0; i < arraySize; ++i) std::cout << rawArray[i] << " ";
    std::cout << "\n\n";

    // Example 3: Sorting via C++ iterator range
    std::vector<uint32_t> rangeData = {99, 11, 44, 22, 77, 33};
    qi::sort(rangeData.begin(), rangeData.end());

    std::cout << "Sorted via iterators: ";
    for (uint32_t x : rangeData) std::cout << x << " ";
    std::cout << "\n";

    return 0;
}
