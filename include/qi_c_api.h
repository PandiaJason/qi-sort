#ifndef QI_C_API_H
#define QI_C_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
  #ifdef QISORT_EXPORTS
    #define QISORT_API __declspec(dllexport)
  #else
    #define QISORT_API __declspec(dllimport)
  #endif
#else
  #define QISORT_API __attribute__((visibility("default")))
#endif

/**
 * @brief Sort a 32-bit unsigned integer array in-place using QI-Sort.
 * 
 * @param data Pointer to the uint32_t array.
 * @param n Number of elements in the array.
 */
QISORT_API void qi_sort_u32(uint32_t* data, size_t n);

/**
 * @brief Senses dataset distribution statistics (entropy, IPR, effective states) without sorting.
 * 
 * @param data Pointer to the uint32_t array.
 * @param n Number of elements in the array.
 * @param out_entropy Pointer to store average Shannon entropy.
 * @param out_ipr Pointer to store Inverse Participation Ratio (IPR).
 * @param out_neff Pointer to store effective occupied states (N_eff).
 * @param out_duplicate_ratio Pointer to store duplicate ratio.
 */
QISORT_API void qi_analyze_u32(
    const uint32_t* data,
    size_t n,
    double* out_entropy,
    double* out_ipr,
    double* out_neff,
    double* out_duplicate_ratio
);

#ifdef __cplusplus
}
#endif

#endif // QI_C_API_H
