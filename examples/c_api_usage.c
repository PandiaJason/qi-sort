/*
===============================================================================
QI-SORT C-API USAGE EXAMPLE
===============================================================================
Demonstrates using the C-ABI interface (qi_c_api.h) for integration with
C, Rust, Go, Java (FFI), Node.js (ffi-napi), or C# (P/Invoke).
===============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../include/qi_c_api.h"

int main() {
    printf("=== QI-Sort C-API Usage Example ===\n\n");

    uint32_t data[] = {10543, 42, 999999, 12, 0, 8881, 100};
    size_t n = sizeof(data) / sizeof(data[0]);

    printf("Original C array : ");
    for (size_t i = 0; i < n; ++i) printf("%u ", data[i]);
    printf("\n");

    // Call C-API sort function
    qi_sort_u32(data, n);

    printf("Sorted C array   : ");
    for (size_t i = 0; i < n; ++i) printf("%u ", data[i]);
    printf("\n\n");

    // Perform C-API analysis
    double entropy, ipr, neff, dup_ratio;
    qi_analyze_u32(data, n, &entropy, &ipr, &neff, &dup_ratio);

    printf("Distribution Analysis via C-API:\n");
    printf("  - Shannon Entropy : %.4f\n", entropy);
    printf("  - IPR Conc.       : %.4f\n", ipr);
    printf("  - Effective Buckets: %.4f\n", neff);
    printf("  - Duplicate Ratio : %.2f%%\n", dup_ratio * 100.0);

    return 0;
}
