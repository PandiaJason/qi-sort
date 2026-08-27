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

// ════════════════════════════════════════════════════════════════════════════
// 1. FLAGSHIP ENGINE: qi::apex ULTIMATE
// ════════════════════════════════════════════════════════════════════════════

QISORT_API void qi_apex_sort_u32(uint32_t* data, size_t n);
QISORT_API void qi_apex_parallel_sort_u32(uint32_t* data, size_t n, unsigned int num_threads);

QISORT_API void qi_apex_sort_i32(int32_t* data, size_t n);
QISORT_API void qi_apex_sort_f32(float* data, size_t n);

QISORT_API void qi_apex_sort_u64(uint64_t* data, size_t n);
QISORT_API void qi_apex_sort_i64(int64_t* data, size_t n);
QISORT_API void qi_apex_sort_f64(double* data, size_t n);

QISORT_API void qi_sort_pairs_u32_u64(uint32_t* keys, uint64_t* payloads, size_t n);

// ════════════════════════════════════════════════════════════════════════════
// 2. PRODUCTION BASELINE: qi::sort (v0.3.61)
// ════════════════════════════════════════════════════════════════════════════

QISORT_API void qi_sort_u32(uint32_t* data, size_t n);
QISORT_API void qi_parallel_sort_u32(uint32_t* data, size_t n, unsigned int num_threads);

QISORT_API void qi_sort_i32(int32_t* data, size_t n);
QISORT_API void qi_sort_f32(float* data, size_t n);
QISORT_API void qi_sort_u64(uint64_t* data, size_t n);
QISORT_API void qi_sort_i64(int64_t* data, size_t n);
QISORT_API void qi_sort_f64(double* data, size_t n);

QISORT_API void qi_radix8_u32(uint32_t* data, size_t n, int allow_shortcuts);
QISORT_API void qi_radix11_u32(uint32_t* data, size_t n, int allow_shortcuts);
QISORT_API void qi_radix16_u32(uint32_t* data, size_t n, int allow_shortcuts);

// ════════════════════════════════════════════════════════════════════════════
// 3. RESEARCH & NOVEL ALGORITHM MODELS
// ════════════════════════════════════════════════════════════════════════════

// QI-FieldSort (100% Non-Radix Continuous Density Field Inversion)
QISORT_API void qi_field_sort_u32(uint32_t* data, size_t n);

// QI-WaveSort (Wavefunction Block Cache)
QISORT_API void qi_wave_sort_u32(uint32_t* data, size_t n);

// QI Partition Sort (Fixed-Point Q32.32 Micro-Buckets)
QISORT_API void qi_partition_sort_u32(uint32_t* data, size_t n);

// QI Turbo Radix (4-Banked Dual-Histogram)
QISORT_API void qi_turbo_sort_u32(uint32_t* data, size_t n);

// ════════════════════════════════════════════════════════════════════════════
// 4. DATASET PROFILING & SENSING
// ════════════════════════════════════════════════════════════════════════════

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
