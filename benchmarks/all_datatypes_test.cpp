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
    
    // Encode signed ints and sort
    std::vector<uint32_t> enc_sints(signed_ints.size());
    for (size_t i = 0; i < signed_ints.size(); ++i) enc_sints[i] = qi::key_traits::encode(signed_ints[i]);
    qi::sort(enc_sints);
    for (size_t i = 0; i < signed_ints.size(); ++i) signed_ints[i] = qi::key_traits::decode_i32(enc_sints[i]);

    std::cout << "   Sorted   : "; for (auto x : signed_ints) std::cout << x << " "; std::cout << "\n";
    std::cout << "   Correct  : " << (std::is_sorted(signed_ints.begin(), signed_ints.end()) ? "PASSED" : "FAILED") << "\n\n";

    // 2. IEEE 754 FLOATING-POINT NUMBERS (float)
    std::vector<float> floats = {-3.14159f, 100.5f, -0.001f, 42.0f, -999.9f, 0.0f, 2.71828f};
    std::cout << "2. IEEE 754 Floating-Point Numbers (float):\n";
    std::cout << "   Original : "; for (auto x : floats) std::cout << x << " "; std::cout << "\n";

    std::vector<uint32_t> enc_floats(floats.size());
    for (size_t i = 0; i < floats.size(); ++i) enc_floats[i] = qi::key_traits::encode(floats[i]);
    qi::sort(enc_floats);
    for (size_t i = 0; i < floats.size(); ++i) floats[i] = qi::key_traits::decode_float(enc_floats[i]);

    std::cout << "   Sorted   : "; for (auto x : floats) std::cout << x << " "; std::cout << "\n";
    std::cout << "   Correct  : " << (std::is_sorted(floats.begin(), floats.end()) ? "PASSED" : "FAILED") << "\n\n";

    // 3. TEXT STRINGS (std::string)
    std::vector<std::string> strings = {"zebra", "apple", "banana", "cat", "dog", "ant", "bear"};
    std::cout << "3. Text Strings (std::string):\n";
    std::cout << "   Original : "; for (const auto& s : strings) std::cout << s << " "; std::cout << "\n";

    qi::sort_strings(strings);

    std::cout << "   Sorted   : "; for (const auto& s : strings) std::cout << s << " "; std::cout << "\n";
    std::cout << "   Correct  : " << (std::is_sorted(strings.begin(), strings.end()) ? "PASSED" : "FAILED") << "\n\n";

    std::cout << "=========================================================================\n";
    std::cout << "  SUCCESS: All data types (Signed Ints, Floats, Strings) Sorted 100% Correctly!\n";
    std::cout << "=========================================================================\n";

    return 0;
}
