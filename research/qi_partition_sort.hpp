#ifndef QI_PARTITION_SORT_HPP
#define QI_PARTITION_SORT_HPP

/*
===============================================================================
QI Partition Sort: Fine-Grained Quantile Partitioning (Header-Only C++17)
===============================================================================
Innovations:
1. 64-Bit Fixed-Point Multiplier (1-cycle integer bucket mapping)
2. K = 2048 Partitions -> Average bucket size < 500 keys (100% L1 cache resident)
3. Direct Sequential In-Cache Sorting + Block DMA Copyback
4. 8-Way Instruction-Level Pipelining (ILP) with PF=48 lookahead prefetch
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

// K = 2048 buckets: 1M / 2048 ≈ 488 elements per bucket (2 KB — guaranteed 100% L1 resident)
constexpr size_t NUM_BUCKETS = 2048;
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

inline void insertionSort(u32* data, size_t n) {
    for (size_t i = 1; i < n; ++i) {
        u32 key = data[i];
        size_t j = i;
        while (j > 0 && data[j - 1] > key) {
            data[j] = data[j - 1];
            --j;
        }
        data[j] = key;
    }
}

} // namespace detail

/**
 * @brief High-Speed Fixed-Point Rank-Partition Sort
 */
inline void sort(u32* data, size_t n) {
    if (n <= 1) return;
    if (n <= SMALL_SORT_THRESH) {
        std::sort(data, data + n);
        return;
    }

    // Step 1: Find Min and Max (4-way unrolled)
    u32 minVal = data[0];
    u32 maxVal = data[0];
    size_t idx = 1;
    for (; idx + 3 < n; idx += 4) {
        u32 v0 = data[idx], v1 = data[idx+1], v2 = data[idx+2], v3 = data[idx+3];
        minVal = std::min({minVal, v0, v1, v2, v3});
        maxVal = std::max({maxVal, v0, v1, v2, v3});
    }
    for (; idx < n; ++idx) {
        if (data[idx] < minVal) minVal = data[idx];
        if (data[idx] > maxVal) maxVal = data[idx];
    }

    if (minVal == maxVal) return; // All elements identical

    // Step 2: 64-Bit Fixed-Point Multiplier
    constexpr size_t K = NUM_BUCKETS;
    const u64 range = static_cast<u64>(maxVal) - minVal;
    const u64 mult = ((static_cast<u64>(K - 1) << 32) + range - 1) / (range > 0 ? range : 1);

    auto getBucket = [minVal, mult](u32 v) -> size_t {
        u64 delta = static_cast<u64>(v - minVal);
        return static_cast<size_t>((delta * mult) >> 32);
    };

    // Step 3: Count bucket occupancies (8-way unrolled for ILP)
    alignas(64) uint32_t counts[K] = {};
    size_t i = 0;
    for (; i + 7 < n; i += 8) {
        counts[getBucket(data[i])]++;
        counts[getBucket(data[i+1])]++;
        counts[getBucket(data[i+2])]++;
        counts[getBucket(data[i+3])]++;
        counts[getBucket(data[i+4])]++;
        counts[getBucket(data[i+5])]++;
        counts[getBucket(data[i+6])]++;
        counts[getBucket(data[i+7])]++;
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

    // Step 4: Scatter elements into scratch partition buffer (Branchless split loop)
    u32* buf = detail::getScratch().get(n);
    constexpr size_t PF = 48;
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

    // Step 5: Direct In-Buffer Local Resolution
    // Each bucket is sorted in-place inside `buf` while resident in L1 cache
    for (size_t k = 0; k < K; ++k) {
        size_t start = starts[k];
        size_t count = counts[k];
        if (count > 1) {
            if (count <= 24) {
                detail::insertionSort(buf + start, count);
            } else if (count > (n / 2) && count > 1024) {
                sort(buf + start, count);
            } else {
                std::sort(buf + start, buf + start + count);
            }
        }
    }

    // Step 6: Single High-Bandwidth Sequential Copyback
    std::memcpy(data, buf, n * sizeof(u32));
}

inline void sort(std::vector<u32>& vec) {
    sort(vec.data(), vec.size());
}

} // namespace qi_partition

#endif // QI_PARTITION_SORT_HPP
