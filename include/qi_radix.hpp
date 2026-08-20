#ifndef QI_RADIX_HPP
#define QI_RADIX_HPP

/*
===============================================================================
QI-RADIX: Quantum-Inspired Adaptive Radix Sorting Library (Header-Only)
===============================================================================

A modular, production-ready C++17 header-only library for ultra-fast adaptive
32-bit integer sorting using probability-amplitude distribution sensing.

Usage:
    #include "qi_radix.hpp"

    std::vector<uint32_t> data = {5, 2, 9, 1, 5, 6};
    qi::sort(data);

    // Or with iterators:
    qi::sort(data.begin(), data.end());

===============================================================================
*/

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <random>
#include <thread>
#include <type_traits>
#include <vector>

namespace qi {

using u32 = uint32_t;
using u64 = uint64_t;

// ============================================================================
// ENUMS & CONFIGURATION
// ============================================================================

enum class Radix : int {
    R8 = 8,
    R11 = 11,
    R16 = 16
};

struct SortOptions {
    size_t sampleSize = 8192;    // Number of elements sampled for state sensing
    bool allowShortcuts = true;   // Enable O(N) early exit for pre-sorted/reverse data
    bool verbose = false;         // Output debug/telemetry information to stdout
    bool parallel = false;        // Enable multi-threaded parallel radix sorting
    unsigned int numThreads = 0;  // 0 = auto-detect (hardware_concurrency)
};

// ============================================================================
// QI STATE VECTOR & METRICS
// ============================================================================

struct ByteState {
    double entropy = 0.0;                 // Shannon entropy normalized [0, 1]
    double amplitudeConcentration = 0.0;  // IPR = sum(|psi_i|^4) = sum(p_i^2)
    double effectiveStates = 0.0;         // N_eff = 1 / IPR
    int occupied = 0;                     // Count of non-zero buckets (out of 256)
    std::array<double, 256> probability{};
    std::array<double, 256> amplitude{};  // psi_i = sqrt(p_i)
};

struct State {
    double averageEntropy = 0.0;
    double amplitudeConcentration = 0.0;  // Average IPR across bytes
    double effectiveStates = 0.0;         // Average N_eff across bytes
    double duplicateRatio = 0.0;          // Fraction of duplicate keys [0, 1]
    double orderedness = 0.0;             // Degree of pre-sorted order [0, 1]
    double disorder = 0.0;
    double lowByteComplexity = 0.0;
    double highByteComplexity = 0.0;
    u32 bitOrSum = 0;
    u32 bitAndSum = ~0u;
    std::array<ByteState, 4> bytes{};
    size_t sampleSize = 0;
    double analysisTimeMs = 0.0;
    Radix recommendedRadix = Radix::R16;
};

// ============================================================================
// INTERNAL DETAIL IMPLEMENTATION
// ============================================================================

namespace detail {

static inline uint8_t getByte(u32 value, int byte) {
    return static_cast<uint8_t>((value >> (byte * 8)) & 0xFFu);
}

static inline double calculateEntropy(const std::array<u64, 256>& counts, size_t n) {
    if (n == 0) return 0.0;
    double h = 0.0;
    for (u64 c : counts) {
        if (c == 0) continue;
        double p = static_cast<double>(c) / static_cast<double>(n);
        h -= p * std::log2(p);
    }
    return h / 8.0;
}


static inline State analyzeData(const u32* data, size_t n, size_t targetSampleSize = 8192) {
    auto start = std::chrono::steady_clock::now();
    State state;
    size_t sampleSize = std::min<size_t>(targetSampleSize, n);
    state.sampleSize = sampleSize;

    std::array<std::array<u64, 256>, 4> counts{};
    u32 bitOr = 0;
    u32 bitAnd = ~0u;
    size_t orderedCount = 0;

    size_t step = std::max<size_t>(1, n / sampleSize);
    u32 prevVal = 0;

    std::vector<u32> sampleBuf(sampleSize);

    for (size_t i = 0; i < sampleSize; ++i) {
        size_t idx = (i * step);
        if (idx >= n) idx = n - 1;
        u32 val = data[idx];
        sampleBuf[i] = val;

        bitOr |= val;
        bitAnd &= val;

        if (i > 0 && prevVal <= val) orderedCount++;
        prevVal = val;

        counts[0][val & 0xFFu]++;
        counts[1][(val >> 8) & 0xFFu]++;
        counts[2][(val >> 16) & 0xFFu]++;
        counts[3][(val >> 24) & 0xFFu]++;
    }

    state.bitOrSum = bitOr;
    state.bitAndSum = bitAnd;
    state.orderedness = (sampleSize > 1) ? static_cast<double>(orderedCount) / (sampleSize - 1) : 1.0;
    state.disorder = 1.0 - state.orderedness;

    std::sort(sampleBuf.begin(), sampleBuf.end());
    size_t uniqueCount = 0;
    for (size_t i = 0; i < sampleSize; ++i) {
        if (i == 0 || sampleBuf[i] != sampleBuf[i - 1]) uniqueCount++;
    }
    state.duplicateRatio = 1.0 - static_cast<double>(uniqueCount) / sampleSize;

    double entropySum = 0.0;
    double concentrationSum = 0.0;
    double effectiveSum = 0.0;

    for (int b = 0; b < 4; ++b) {
        ByteState& bs = state.bytes[b];
        bs.entropy = calculateEntropy(counts[b], sampleSize);

        double conc = 0.0;
        int occ = 0;
        for (int i = 0; i < 256; ++i) {
            if (counts[b][i] == 0) continue;
            occ++;
            double p = static_cast<double>(counts[b][i]) / sampleSize;
            bs.probability[i] = p;
            bs.amplitude[i] = std::sqrt(p);
            conc += p * p;
        }
        bs.amplitudeConcentration = conc;
        bs.effectiveStates = (conc > 0.0) ? (1.0 / conc) : 0.0;
        bs.occupied = occ;

        entropySum += bs.entropy;
        concentrationSum += conc;
        effectiveSum += bs.effectiveStates;
    }

    state.averageEntropy = entropySum / 4.0;
    state.amplitudeConcentration = concentrationSum / 4.0;
    state.effectiveStates = effectiveSum / 4.0;
    state.lowByteComplexity = (state.bytes[0].entropy + state.bytes[1].entropy) / 2.0;
    state.highByteComplexity = (state.bytes[2].entropy + state.bytes[3].entropy) / 2.0;

    // QI Cache-Thrashing & Pass-Count Cost Model Selection
    // Baseline orderedness for uniform random data is 0.50 (coin-flip increasing pairs).
    // True spatial order factor above random chance: 0.50 -> 0.0, 1.0 -> 1.0.
    double orderFactor = std::max(0.0, (state.orderedness - 0.50) * 2.0);

    // R16 count array = 65,536 entries = 512 KB (exceeds L1/L2 cache).
    // L1/L2 cache misses occur when bucket dispersion (N_eff) is high AND data lacks spatial ordering.
    double r16BucketDispersion = (state.bytes[0].effectiveStates * state.bytes[1].effectiveStates) / 65536.0;
    double r16CachePenalty = (r16BucketDispersion > 0.20) ? (r16BucketDispersion * state.averageEntropy * (1.0 - orderFactor) * 1.2) : 0.0;
    double r16Passes = (state.bitOrSum <= 0xFFFFu) ? 1.0 : 2.0;
    double costR16 = n * (r16Passes + r16CachePenalty);

    // R11 uses 3 passes. Bucket array = 2,048 entries = 16 KB (fits in 32 KB L1 Data Cache).
    // Zero cache penalty, but incurs 3 memory passes over N items.
    double costR11 = n * 3.0;

    // R8 uses 4 passes. Bucket array = 256 entries = 2 KB (fits in L1 Data Cache).
    double costR8  = n * 4.0;

    if (state.orderedness > 0.98) { costR16 *= 0.05; costR11 *= 0.05; costR8 *= 0.05; }

    if (state.duplicateRatio > 0.90) {
        state.recommendedRadix = Radix::R16;
    } else if (costR11 < costR16 && costR11 < costR8) {
        state.recommendedRadix = Radix::R11;
    } else if (costR8 < costR16 && costR8 < costR11) {
        state.recommendedRadix = Radix::R8;
    } else {
        state.recommendedRadix = Radix::R16;
    }

    auto end = std::chrono::steady_clock::now();
    state.analysisTimeMs = std::chrono::duration<double, std::milli>(end - start).count();

    return state;
}

// Ultra-fast Radix-16 implementation
static inline void radixSort16(u32* data, size_t n, bool allowShortcuts) {
    if (n <= 1) return;

    if (allowShortcuts) {
        if (std::is_sorted(data, data + n)) return;
        bool isReverse = true;
        for (size_t i = 1; i < std::min<size_t>(n, 1024); ++i) {
            if (data[i - 1] < data[i]) { isReverse = false; break; }
        }
        if (isReverse && std::is_sorted(std::make_reverse_iterator(data + n), std::make_reverse_iterator(data))) {
            std::reverse(data, data + n);
            return;
        }
    }

    std::vector<u32> temp(n);
    u32* src = data;
    u32* dst = temp.data();

    std::unique_ptr<std::array<size_t, 65536>> count0_ptr(new std::array<size_t, 65536>());
    std::unique_ptr<std::array<size_t, 65536>> count1_ptr(new std::array<size_t, 65536>());
    auto& count0 = *count0_ptr;
    auto& count1 = *count1_ptr;

    count0.fill(0);
    count1.fill(0);

    for (size_t i = 0; i < n; ++i) {
        u32 val = src[i];
        count0[val & 0xFFFFu]++;
        count1[val >> 16]++;
    }

    bool singlePass = (count1[0] == n);

    if (singlePass) {
        size_t sum = 0;
        for (int i = 0; i < 65536; ++i) {
            size_t c = count0[i];
            count0[i] = sum;
            sum += c;
        }
        for (size_t i = 0; i < n; ++i) {
            u32 val = src[i];
            dst[count0[val & 0xFFFFu]++] = val;
        }
        std::memcpy(data, dst, n * sizeof(u32));
        return;
    }

    size_t sum0 = 0, sum1 = 0;
    for (int i = 0; i < 65536; ++i) {
        size_t c0 = count0[i]; count0[i] = sum0; sum0 += c0;
        size_t c1 = count1[i]; count1[i] = sum1; sum1 += c1;
    }

    size_t i = 0;
    for (; i + 3 < n; i += 4) {
        u32 v0 = src[i], v1 = src[i+1], v2 = src[i+2], v3 = src[i+3];
        #if defined(__GNUC__) || defined(__clang__)
        __builtin_prefetch(&src[i + 32], 0, 1);
        #endif
        dst[count0[v0 & 0xFFFFu]++] = v0;
        dst[count0[v1 & 0xFFFFu]++] = v1;
        dst[count0[v2 & 0xFFFFu]++] = v2;
        dst[count0[v3 & 0xFFFFu]++] = v3;
    }
    for (; i < n; ++i) {
        u32 v = src[i];
        dst[count0[v & 0xFFFFu]++] = v;
    }

    i = 0;
    for (; i + 3 < n; i += 4) {
        u32 v0 = dst[i], v1 = dst[i+1], v2 = dst[i+2], v3 = dst[i+3];
        #if defined(__GNUC__) || defined(__clang__)
        __builtin_prefetch(&dst[i + 32], 0, 1);
        #endif
        src[count1[v0 >> 16]++] = v0;
        src[count1[v1 >> 16]++] = v1;
        src[count1[v2 >> 16]++] = v2;
        src[count1[v3 >> 16]++] = v3;
    }
    for (; i < n; ++i) {
        u32 v = dst[i];
        src[count1[v >> 16]++] = v;
    }
}

// Radix-11 implementation
static inline void radixSort11(u32* data, size_t n, bool allowShortcuts) {
    if (n <= 1) return;
    if (allowShortcuts && std::is_sorted(data, data + n)) return;

    std::vector<u32> temp(n);
    u32* src = data;
    u32* dst = temp.data();

    int shift = 0;
    while (shift < 32) {
        int currentBits = std::min(11, 32 - shift);
        uint32_t buckets = 1u << currentBits;
        std::vector<size_t> count(buckets, 0);
        uint32_t mask = buckets - 1;

        for (size_t i = 0; i < n; ++i) count[(src[i] >> shift) & mask]++;

        size_t sum = 0;
        for (uint32_t i = 0; i < buckets; ++i) {
            size_t c = count[i]; count[i] = sum; sum += c;
        }

        for (size_t i = 0; i < n; ++i) {
            uint32_t digit = (src[i] >> shift) & mask;
            dst[count[digit]++] = src[i];
        }
        std::swap(src, dst);
        shift += currentBits;
    }

    if (src != data) std::memcpy(data, src, n * sizeof(u32));
}

// Radix-8 implementation
static inline void radixSort8(u32* data, size_t n, bool allowShortcuts) {
    if (n <= 1) return;
    if (allowShortcuts && std::is_sorted(data, data + n)) return;

    std::vector<u32> temp(n);
    u32* src = data;
    u32* dst = temp.data();

    for (int shift = 0; shift < 32; shift += 8) {
        alignas(64) size_t count[256] = {0};
        for (size_t i = 0; i < n; ++i) count[(src[i] >> shift) & 0xFFu]++;

        size_t sum = 0;
        for (int i = 0; i < 256; ++i) {
            size_t c = count[i]; count[i] = sum; sum += c;
        }

        for (size_t i = 0; i < n; ++i) {
            u32 digit = (src[i] >> shift) & 0xFFu;
            dst[count[digit]++] = src[i];
        }
        std::swap(src, dst);
    }
    if (src != data) std::memcpy(data, src, n * sizeof(u32));
}

// ============================================================================
// PARALLEL RADIX SORT ENGINE (multi-threaded histogram + scatter)
// ============================================================================

static inline void parallelRadixPass(u32* src, u32* dst, size_t n,
                                      int shift, int bits, unsigned int numThreads) {
    uint32_t buckets = 1u << bits;
    uint32_t mask = buckets - 1;

    // Phase 1: Parallel local histograms
    std::vector<std::vector<size_t>> localCounts(numThreads, std::vector<size_t>(buckets, 0));
    size_t chunk = n / numThreads;
    std::vector<std::thread> threads;

    for (unsigned t = 0; t < numThreads; t++) {
        size_t start = t * chunk;
        size_t end = (t == numThreads - 1) ? n : start + chunk;
        threads.emplace_back([&, start, end, t]() {
            auto& lc = localCounts[t];
            for (size_t i = start; i < end; i++)
                lc[(src[i] >> shift) & mask]++;
        });
    }
    for (auto& th : threads) th.join();

    // Phase 2: Global prefix sum (serial — fast on small bucket arrays)
    std::vector<size_t> globalCount(buckets, 0);
    for (unsigned t = 0; t < numThreads; t++)
        for (uint32_t b = 0; b < buckets; b++)
            globalCount[b] += localCounts[t][b];

    std::vector<size_t> globalOffsets(buckets);
    size_t offset = 0;
    for (uint32_t b = 0; b < buckets; b++) {
        globalOffsets[b] = offset;
        offset += globalCount[b];
    }

    // Phase 3: Per-thread scatter offsets
    std::vector<std::vector<size_t>> threadOffsets(numThreads, std::vector<size_t>(buckets, 0));
    for (uint32_t b = 0; b < buckets; b++) {
        threadOffsets[0][b] = globalOffsets[b];
        for (unsigned t = 1; t < numThreads; t++)
            threadOffsets[t][b] = threadOffsets[t-1][b] + localCounts[t-1][b];
    }

    // Phase 4: Parallel scatter
    threads.clear();
    for (unsigned t = 0; t < numThreads; t++) {
        size_t start = t * chunk;
        size_t end = (t == numThreads - 1) ? n : start + chunk;
        threads.emplace_back([&, start, end, t]() {
            auto& to = threadOffsets[t];
            for (size_t i = start; i < end; i++)
                dst[to[(src[i] >> shift) & mask]++] = src[i];
        });
    }
    for (auto& th : threads) th.join();
}

static inline void parallelRadixSort16(u32* data, size_t n, bool allowShortcuts,
                                        unsigned int numThreads) {
    if (n <= 1) return;

    if (allowShortcuts) {
        if (std::is_sorted(data, data + n)) return;
        bool isReverse = true;
        for (size_t i = 1; i < std::min<size_t>(n, 1024); ++i) {
            if (data[i - 1] < data[i]) { isReverse = false; break; }
        }
        if (isReverse && std::is_sorted(std::make_reverse_iterator(data + n),
                                        std::make_reverse_iterator(data))) {
            std::reverse(data, data + n);
            return;
        }
    }

    std::vector<u32> temp(n);

    // Pass 1: lower 16 bits
    parallelRadixPass(data, temp.data(), n, 0, 16, numThreads);
    // Pass 2: upper 16 bits
    parallelRadixPass(temp.data(), data, n, 16, 16, numThreads);
}

static inline void parallelRadixSort11(u32* data, size_t n, bool allowShortcuts,
                                        unsigned int numThreads) {
    if (n <= 1) return;
    if (allowShortcuts && std::is_sorted(data, data + n)) return;

    std::vector<u32> temp(n);
    u32* src = data;
    u32* dst = temp.data();

    int shift = 0;
    while (shift < 32) {
        int currentBits = std::min(11, 32 - shift);
        parallelRadixPass(src, dst, n, shift, currentBits, numThreads);
        std::swap(src, dst);
        shift += currentBits;
    }

    if (src != data) std::memcpy(data, src, n * sizeof(u32));
}

static inline void parallelRadixSort8(u32* data, size_t n, bool allowShortcuts,
                                       unsigned int numThreads) {
    if (n <= 1) return;
    if (allowShortcuts && std::is_sorted(data, data + n)) return;

    std::vector<u32> temp(n);
    u32* src = data;
    u32* dst = temp.data();

    for (int shift = 0; shift < 32; shift += 8) {
        parallelRadixPass(src, dst, n, shift, 8, numThreads);
        std::swap(src, dst);
    }
    if (src != data) std::memcpy(data, src, n * sizeof(u32));
}

} // namespace detail

// ============================================================================
// PUBLIC API FUNCTIONS
// ============================================================================

/**
 * @brief Analyze the quantum-inspired distribution state of an integer dataset without sorting.
 */
inline State analyze(const u32* data, size_t n, size_t sampleSize = 8192) {
    return detail::analyzeData(data, n, sampleSize);
}

/**
 * @brief Analyze the quantum-inspired distribution state of a std::vector without sorting.
 */
inline State analyze(const std::vector<u32>& data, size_t sampleSize = 8192) {
    return detail::analyzeData(data.data(), data.size(), sampleSize);
}

/**
 * @brief Perform Quantum-Inspired Adaptive Radix Sort on a raw pointer range [data, data + n).
 */
inline void sort(u32* data, size_t n, SortOptions options = SortOptions{}) {
    if (n <= 1) return;
    State state = detail::analyzeData(data, n, options.sampleSize);

    const char* modeStr = "scalar";
    if (options.parallel && n >= 100000) {
        unsigned int threads = options.numThreads;
        if (threads == 0) threads = std::thread::hardware_concurrency();
        if (threads < 2) threads = 2;
        modeStr = "parallel";

        if (options.verbose) {
            std::cout << "[QI-Radix] N=" << n
                      << " | Mode=PARALLEL (" << threads << " threads)"
                      << " | Selected=" << (state.recommendedRadix == Radix::R16 ? "RADIX-16" : (state.recommendedRadix == Radix::R11 ? "RADIX-11" : "RADIX-8"))
                      << " | Entropy=" << state.averageEntropy
                      << " | EffectiveStates=" << state.effectiveStates << "\n";
        }

        switch (state.recommendedRadix) {
            case Radix::R8:  detail::parallelRadixSort8(data, n, options.allowShortcuts, threads); break;
            case Radix::R11: detail::parallelRadixSort11(data, n, options.allowShortcuts, threads); break;
            case Radix::R16: detail::parallelRadixSort16(data, n, options.allowShortcuts, threads); break;
        }
    } else {
        if (options.verbose) {
            std::cout << "[QI-Radix] N=" << n
                      << " | Mode=SCALAR"
                      << " | Selected=" << (state.recommendedRadix == Radix::R16 ? "RADIX-16" : (state.recommendedRadix == Radix::R11 ? "RADIX-11" : "RADIX-8"))
                      << " | Entropy=" << state.averageEntropy
                      << " | EffectiveStates=" << state.effectiveStates << "\n";
        }

        switch (state.recommendedRadix) {
            case Radix::R8:  detail::radixSort8(data, n, options.allowShortcuts); break;
            case Radix::R11: detail::radixSort11(data, n, options.allowShortcuts); break;
            case Radix::R16: detail::radixSort16(data, n, options.allowShortcuts); break;
        }
    }
}

/**
 * @brief Perform Quantum-Inspired Adaptive Radix Sort on a std::vector<uint32_t>.
 */
inline void sort(std::vector<u32>& data, SortOptions options = SortOptions{}) {
    sort(data.data(), data.size(), options);
}

/**
 * @brief Perform Quantum-Inspired Adaptive Radix Sort on a random-access iterator range [begin, end).
 */
template <typename RandomIt>
inline void sort(RandomIt begin, RandomIt end, SortOptions options = SortOptions{}) {
    static_assert(std::is_same<typename std::iterator_traits<RandomIt>::value_type, u32>::value,
                  "qi::sort requires iterators over 32-bit unsigned integers (uint32_t)");
    size_t n = std::distance(begin, end);
    if (n <= 1) return;
    u32* first = &(*begin);
    sort(first, n, options);
}

} // namespace qi

#endif // QI_RADIX_HPP
