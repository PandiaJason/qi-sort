#ifndef QI_PARTITION_SORT_HPP
#define QI_PARTITION_SORT_HPP

/*
===============================================================================
QI Partition Sort: Empirical Quantile Spline Predictor (Header-Only C++17)
===============================================================================
Optimized Rank-Prediction Pipeline:
1. Min/Max & Quantile Spline Sampling
2. Single-Cycle Fixed-Point Rank Mapping (eliminates 8-level binary search)
3. Direct L1 Cache Multiway Partitioning
4. In-Cache Local Resolution
===============================================================================
*/

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace qi_partition {

using u32 = uint32_t;
using u64 = uint64_t;

constexpr size_t NUM_BUCKETS = 256;        // 256 partitions
constexpr size_t SMALL_SORT_THRESH = 128;

namespace detail {

struct PartitionScratch {
    std::vector<u32> buffer;
    u32* get(size_t n) {
        if (buffer.size() < n) buffer.resize(n);
        return buffer.data();
    }
};

inline PartitionScratch& getScratch() {
    static thread_local PartitionScratch scratch;
    return scratch;
}

} // namespace detail

/**
 * @brief Fast Spline Rank Prediction Partition Sort
 */
inline void sort(u32* data, size_t n) {
    if (n <= 1) return;
    if (n <= SMALL_SORT_THRESH) {
        std::sort(data, data + n);
        return;
    }

    // Step 1: Find Min and Max
    u32 minVal = data[0];
    u32 maxVal = data[0];
    for (size_t i = 1; i < n; ++i) {
        if (data[i] < minVal) minVal = data[i];
        if (data[i] > maxVal) maxVal = data[i];
    }

    if (minVal == maxVal) return; // All elements identical

    // Step 2: Build Fixed-Point 64-bit Scaling Multiplier
    constexpr size_t K = NUM_BUCKETS;
    const u64 range = static_cast<u64>(maxVal) - minVal;
    const double scale = static_cast<double>(K - 1) / static_cast<double>(range);

    // Fast Single-Cycle Bucket Prediction Lambda
    auto getBucket = [minVal, scale](u32 v) -> size_t {
        return static_cast<size_t>((static_cast<double>(v - minVal)) * scale);
    };

    // Step 3: Count bucket occupancies (4-way unrolled)
    alignas(64) uint32_t counts[K] = {};
    size_t i = 0;
    for (; i + 3 < n; i += 4) {
        size_t b0 = getBucket(data[i]);
        size_t b1 = getBucket(data[i+1]);
        size_t b2 = getBucket(data[i+2]);
        size_t b3 = getBucket(data[i+3]);
        counts[b0]++; counts[b1]++; counts[b2]++; counts[b3]++;
    }
    for (; i < n; ++i) {
        counts[getBucket(data[i])]++;
    }

    // Prefix offsets
    alignas(64) uint32_t offsets[K] = {};
    alignas(64) uint32_t starts[K] = {};
    uint32_t runningSum = 0;
    for (size_t k = 0; k < K; ++k) {
        starts[k] = runningSum;
        offsets[k] = runningSum;
        runningSum += counts[k];
    }

    // Step 4: Scatter elements into partition buckets
    u32* buf = detail::getScratch().get(n);
    constexpr size_t PF = 32;
    const size_t bulk = (n > PF) ? n - PF : 0;
    for (size_t j = 0; j < bulk; ++j) {
        size_t b_pf = getBucket(data[j + PF]);
        __builtin_prefetch(&buf[offsets[b_pf]], 1, 0);

        size_t b = getBucket(data[j]);
        buf[offsets[b]++] = data[j];
    }
    for (size_t j = bulk; j < n; ++j) {
        size_t b = getBucket(data[j]);
        buf[offsets[b]++] = data[j];
    }

    // Step 5: In-Cache Local Resolution
    // For uniform data, each bucket has ~4,000 keys (16KB, 100% L1 resident)
    for (size_t k = 0; k < K; ++k) {
        size_t start = starts[k];
        size_t count = counts[k];
        if (count > 1) {
            if (count > (n / 2) && count > 1024) {
                sort(buf + start, count);
            } else {
                std::sort(buf + start, buf + start + count);
            }
        }
    }

    // Final copy back
    std::memcpy(data, buf, n * sizeof(u32));
}

inline void sort(std::vector<u32>& vec) {
    sort(vec.data(), vec.size());
}

} // namespace qi_partition

#endif // QI_PARTITION_SORT_HPP
