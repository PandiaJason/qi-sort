#if __has_include("qi_c_api.h")
#include "qi_c_api.h"
#include "qi_radix.hpp"
#else
#include "../include/qi_c_api.h"
#include "../include/qi_radix.hpp"
#endif

extern "C" {

void qi_sort_u32(uint32_t* data, size_t n) {
    if (!data || n <= 1) return;
    qi::sort(data, n);
}

void qi_parallel_sort_u32(uint32_t* data, size_t n, unsigned int num_threads) {
    if (!data || n <= 1) return;
    qi::SortOptions opts;
    opts.parallel = true;
    opts.numThreads = num_threads;
    qi::sort(data, n, opts);
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
    qi::detail::radixSort16(data, n, allow_shortcuts != 0);
}

void qi_sort_i32(int32_t* data, size_t n) {
    if (!data || n <= 1) return;
    qi::sort(data, n);
}

void qi_parallel_sort_i32(int32_t* data, size_t n, unsigned int num_threads) {
    if (!data || n <= 1) return;
    qi::SortOptions opts;
    opts.parallel = true;
    opts.numThreads = num_threads;
    qi::sort(data, n, opts);
}

void qi_sort_f32(float* data, size_t n) {
    if (!data || n <= 1) return;
    qi::sort(data, n);
}

void qi_parallel_sort_f32(float* data, size_t n, unsigned int num_threads) {
    if (!data || n <= 1) return;
    qi::SortOptions opts;
    opts.parallel = true;
    opts.numThreads = num_threads;
    qi::sort(data, n, opts);
}

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

}
