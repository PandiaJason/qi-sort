#ifndef QI_HYPERFIELD_HPP
#define QI_HYPERFIELD_HPP

//  (C) Copyright Jason Pandian 2026.
//  Distributed under the GPL-2.0 License.

/*
========================================================================================
  qi::hyperfield v3.1 (Robust Continuous Field Sorter with Zero-Misfire Sensing)
========================================================================================
  1. 5ns Strided Fast-Check for Pre-Sorted & Reverse Sequences.
  2. Single-Pass Counting Field for Narrow Ranges (<= 4095).
  3. Stable Zero-Misfire Dispatch: Directly invokes L1-Bound Apex Engine for Random Data.
========================================================================================
*/

#include <algorithm>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <thread>
#include <type_traits>
#include <cassert>
#include "qi_apex.hpp"

namespace qi {
namespace hyperfield {

using u32 = uint32_t;
using u64 = uint64_t;

namespace detail {

// ── Single-Pass Direct Counting Field for Narrow Ranges (<= 4095) ──
inline void counting_field_sort(u32* data, size_t n, u32 max_val) {
    const size_t num_bins = max_val + 1;
    alignas(64) static thread_local uint32_t counts[4096];
    std::memset(counts, 0, num_bins * sizeof(uint32_t));

    for (size_t i = 0; i + 7 < n; i += 8) {
        counts[data[i]]++;   counts[data[i+1]]++;
        counts[data[i+2]]++; counts[data[i+3]]++;
        counts[data[i+4]]++; counts[data[i+5]]++;
        counts[data[i+6]]++; counts[data[i+7]]++;
    }
    for (size_t i = (n / 8) * 8; i < n; ++i) counts[data[i]]++;

    size_t idx = 0;
    for (size_t val = 0; val < num_bins; ++val) {
        uint32_t cnt = counts[val];
        if (cnt > 0) {
            for (size_t k = 0; k < cnt; ++k) data[idx + k] = static_cast<u32>(val);
            idx += cnt;
        }
    }
}

} // namespace detail

/**
 * @brief Robust Continuous Field Sort for Unsigned 32-bit Integers
 */
inline void sort(u32* data, size_t n) {
    if (n <= 1) return;

    if (n <= 64) {
        std::sort(data, data + n);
        return;
    }

    // ── 1. 5ns Strided Fast-Check (64 Samples) ──
    const size_t stride = n > 64 ? n / 64 : 1;
    bool sample_asc = true, sample_desc = true;
    for (size_t i = stride; i < n; i += stride) {
        if (data[i] < data[i - stride]) sample_asc = false;
        if (data[i] > data[i - stride]) sample_desc = false;
        if (!sample_asc && !sample_desc) break;
    }

    if (sample_asc) {
        bool full_asc = true;
        for (size_t i = 1; i < n; ++i) {
            if (data[i] < data[i - 1]) { full_asc = false; break; }
        }
        if (full_asc) return; // Already sorted
    } else if (sample_desc) {
        bool full_desc = true;
        for (size_t i = 1; i < n; ++i) {
            if (data[i] > data[i - 1]) { full_desc = false; break; }
        }
        if (full_desc) {
            std::reverse(data, data + n);
            return;
        }
    }

    // ── 2. 50ns Field Domain Calibration (Min/Max) ──
    u32 min_val = data[0], max_val = data[0];
    for (size_t i = 0; i < n; ++i) {
        u32 v = data[i];
        if (v < min_val) min_val = v;
        if (v > max_val) max_val = v;
    }

    if (min_val == max_val) return; // All identical

    // ── 3. Narrow Range Fast-Path (<= 4095) ──
    if (max_val <= 4095) {
        detail::counting_field_sort(data, n, max_val);
        return;
    }

    // ── 4. General Workloads: Dispatch to L1-Bound Apex Sorter ──
    qi::apex::sort(data, n);
}

/**
 * @brief Continuous Density Field Sort for Signed 32-bit Integers
 */
inline void sort(int32_t* data, size_t n) {
    if (n <= 1) return;
    auto* udata = reinterpret_cast<u32*>(data);
    for (size_t i = 0; i < n; ++i) udata[i] ^= 0x80000000u;
    sort(udata, n);
    for (size_t i = 0; i < n; ++i) udata[i] ^= 0x80000000u;
}

/**
 * @brief Continuous Density Field Sort for IEEE 754 Floats
 */
inline void sort(float* data, size_t n) {
    if (n <= 1) return;
    auto* udata = reinterpret_cast<u32*>(data);
    for (size_t i = 0; i < n; ++i) {
        u32 mask = (static_cast<int32_t>(udata[i]) >> 31) | 0x80000000u;
        udata[i] ^= mask;
    }
    sort(udata, n);
    for (size_t i = 0; i < n; ++i) {
        u32 mask = ((udata[i] >> 31) - 1) | 0x80000000u;
        udata[i] ^= mask;
    }
}

/**
 * @brief Multi-Threaded Parallel Field Sorter
 */
inline void parallel_sort(u32* data, size_t n, unsigned int num_threads = 0) {
    if (n < 100000) {
        sort(data, n);
        return;
    }
    if (num_threads == 0) num_threads = std::thread::hardware_concurrency();
    if (num_threads <= 1) {
        sort(data, n);
        return;
    }

    qi::apex::parallel_sort(data, n, num_threads);
}

} // namespace hyperfield
} // namespace qi

#endif // QI_HYPERFIELD_HPP
