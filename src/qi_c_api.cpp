#include "../include/qi_c_api.h"
#include "../include/qi_apex.hpp"
#include "../include/qi_radix.hpp"
#include "../research/qi_field_sort.hpp"
#include "../research/qi_wave_sort.hpp"
#include "../research/qi_partition_sort.hpp"
#include "../research/qi_turbo_radix.hpp"

extern "C" {

// ── 1. FLAGSHIP: qi::apex ULTIMATE ──
void qi_apex_sort_u32(uint32_t* data, size_t n) {
    if (!data || n <= 1) return;
    qi::apex::sort(data, n);
}

void qi_apex_parallel_sort_u32(uint32_t* data, size_t n, unsigned int num_threads) {
    if (!data || n <= 1) return;
    qi::apex::parallel_sort(data, n, num_threads);
}

void qi_apex_sort_i32(int32_t* data, size_t n) {
    if (!data || n <= 1) return;
    qi::apex::sort(data, n);
}

void qi_apex_sort_f32(float* data, size_t n) {
    if (!data || n <= 1) return;
    qi::apex::sort(data, n);
}

void qi_apex_sort_u64(uint64_t* data, size_t n) {
    if (!data || n <= 1) return;
    qi::apex::sort(data, n);
}

void qi_apex_sort_i64(int64_t* data, size_t n) {
    if (!data || n <= 1) return;
    qi::apex::sort(data, n);
}

void qi_apex_sort_f64(double* data, size_t n) {
    if (!data || n <= 1) return;
    qi::apex::sort(data, n);
}

void qi_sort_pairs_u32_u64(uint32_t* keys, uint64_t* payloads, size_t n) {
    if (!keys || !payloads || n <= 1) return;
    qi::apex::sort_pairs(keys, payloads, n);
}

// ── 2. BASELINE: qi::sort ──
void qi_sort_u32(uint32_t* data, size_t n) {
    if (!data || n <= 1) return;
    qi::apex::sort(data, n); // defaults to fastest apex engine
}

void qi_parallel_sort_u32(uint32_t* data, size_t n, unsigned int num_threads) {
    if (!data || n <= 1) return;
    qi::apex::parallel_sort(data, n, num_threads);
}

void qi_sort_i32(int32_t* data, size_t n) {
    if (!data || n <= 1) return;
    qi::apex::sort(data, n);
}

void qi_sort_f32(float* data, size_t n) {
    if (!data || n <= 1) return;
    qi::apex::sort(data, n);
}

void qi_sort_u64(uint64_t* data, size_t n) {
    if (!data || n <= 1) return;
    qi::apex::sort(data, n);
}

void qi_sort_i64(int64_t* data, size_t n) {
    if (!data || n <= 1) return;
    qi::apex::sort(data, n);
}

void qi_sort_f64(double* data, size_t n) {
    if (!data || n <= 1) return;
    qi::apex::sort(data, n);
}

void qi_radix8_u32(uint32_t* data, size_t n, int allow_shortcuts) {
    if (!data || n <= 1) return;
    qi::detail::radixSort8(data, n, allow_shortcuts != 0);
}

void qi_radix11_u32(uint32_t* data, size_t n, int allow_shortcuts) {
    if (!data || n <= 1) return;
    qi::detail::radixSort11(data, n, allow_shortcuts != 0);
}

void qi_radix16_u32(uint32_t* data, size_t n, int allow_shortcuts) {
    if (!data || n <= 1) return;
    qi::detail::radixSort16(data, n);
}

// ── 3. RESEARCH & NOVEL MODELS ──
void qi_field_sort_u32(uint32_t* data, size_t n) {
    if (!data || n <= 1) return;
    qi_field::sort(data, n);
}

void qi_wave_sort_u32(uint32_t* data, size_t n) {
    if (!data || n <= 1) return;
    qi_wave::sort(data, n);
}

void qi_partition_sort_u32(uint32_t* data, size_t n) {
    if (!data || n <= 1) return;
    qi_partition::sort(data, n);
}

void qi_turbo_sort_u32(uint32_t* data, size_t n) {
    if (!data || n <= 1) return;
    qi_turbo::sort(data, n);
}

// ── 4. SENSING PROFILER ──
void qi_analyze_u32(
    const uint32_t* data,
    size_t n,
    double* out_entropy,
    double* out_ipr,
    double* out_neff,
    double* out_duplicate_ratio
) {
    if (!data || n == 0) return;
    qi::State state = qi::analyze(data, n);
    if (out_entropy) *out_entropy = state.averageEntropy;
    if (out_ipr) *out_ipr = state.amplitudeConcentration;
    if (out_neff) *out_neff = state.effectiveStates;
    if (out_duplicate_ratio) *out_duplicate_ratio = state.duplicateRatio;
}

} // extern "C"
