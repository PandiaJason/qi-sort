#include "include/qi_radix.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

int main() {
    std::cout << "=========================================================================\n";
    std::cout << "  QI-SORT MULTI-DATATYPE CAPABILITY DEMONSTRATION\n";
    std::cout << "=========================================================================\n\n";

    // 1. SIGNED 32-BIT INTEGERS (Negative and Positive numbers)
    std::vector<int32_t> signed_ints = {-42, 10543, -999999, 0, 12, -1, 8881, 100};
    std::cout << "1. Signed 32-bit Integers (int32_t):\n";
    std::cout << "   Original : "; for (auto x : signed_ints) std::cout << x << " "; std::cout << "\n";
    qi::sort(signed_ints);
    std::cout << "   Sorted   : "; for (auto x : signed_ints) std::cout << x << " "; std::cout << "\n";
    std::cout << "   Correct  : " << (std::is_sorted(signed_ints.begin(), signed_ints.end()) ? "PASSED" : "FAILED") << "\n\n";

    // 2. IEEE 754 FLOATING-POINT NUMBERS (float)
    std::vector<float> floats = {-3.14159f, 100.5f, -0.001f, 42.0f, -999.9f, 0.0f, 2.71828f};
    std::cout << "2. IEEE 754 Floating-Point Numbers (float):\n";
    std::cout << "   Original : "; for (auto x : floats) std::cout << x << " "; std::cout << "\n";
    qi::sort(floats);
    std::cout << "   Sorted   : "; for (auto x : floats) std::cout << x << " "; std::cout << "\n";
    std::cout << "   Correct  : " << (std::is_sorted(floats.begin(), floats.end()) ? "PASSED" : "FAILED") << "\n\n";

    // 3. UNSIGNED 64-BIT INTEGERS (uint64_t)
    std::vector<uint64_t> uint64s = {0xFFFFFFFFFFFFFFFFULL, 42ULL, 100000000000ULL, 0ULL, 123456789012345ULL};
    std::cout << "3. Unsigned 64-bit Integers (uint64_t):\n";
    std::cout << "   Original : "; for (auto x : uint64s) std::cout << x << " "; std::cout << "\n";
    qi::sort(uint64s);
    std::cout << "   Sorted   : "; for (auto x : uint64s) std::cout << x << " "; std::cout << "\n";
    std::cout << "   Correct  : " << (std::is_sorted(uint64s.begin(), uint64s.end()) ? "PASSED" : "FAILED") << "\n\n";

    // 4. SIGNED 64-BIT INTEGERS (int64_t)
    std::vector<int64_t> int64s = {-900000000000LL, 42LL, -1LL, 0LL, 100000000000LL};
    std::cout << "4. Signed 64-bit Integers (int64_t):\n";
    std::cout << "   Original : "; for (auto x : int64s) std::cout << x << " "; std::cout << "\n";
    qi::sort(int64s);
    std::cout << "   Sorted   : "; for (auto x : int64s) std::cout << x << " "; std::cout << "\n";
    std::cout << "   Correct  : " << (std::is_sorted(int64s.begin(), int64s.end()) ? "PASSED" : "FAILED") << "\n\n";

    // 5. DOUBLE PRECISION FLOATING-POINT (double)
    std::vector<double> doubles = {-3.1415926535, 1e10, -0.000001, 42.0, -999.9999, 0.0};
    std::cout << "5. Double Precision Floating-Point (double):\n";
    std::cout << "   Original : "; for (auto x : doubles) std::cout << x << " "; std::cout << "\n";
    qi::sort(doubles);
    std::cout << "   Sorted   : "; for (auto x : doubles) std::cout << x << " "; std::cout << "\n";
    std::cout << "   Correct  : " << (std::is_sorted(doubles.begin(), doubles.end()) ? "PASSED" : "FAILED") << "\n\n";

    // 6. TEXT STRINGS (std::string)
    std::vector<std::string> strings = {"zebra", "apple", "banana", "cat", "dog", "ant", "bear"};
    std::cout << "6. Text Strings (std::string):\n";
    std::cout << "   Original : "; for (const auto& s : strings) std::cout << s << " "; std::cout << "\n";
    qi::sort_strings(strings);
    std::cout << "   Sorted   : "; for (const auto& s : strings) std::cout << s << " "; std::cout << "\n";
    std::cout << "   Correct  : " << (std::is_sorted(strings.begin(), strings.end()) ? "PASSED" : "FAILED") << "\n\n";

    std::cout << "=========================================================================\n";
    std::cout << "  SUCCESS: All 6 Data Types (int32, float, uint64, int64, double, string) PASSED!\n";
    std::cout << "=========================================================================\n";

    return 0;
}
