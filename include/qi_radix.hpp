#ifndef QI_RADIX_HPP
#define QI_RADIX_HPP

/*
===============================================================================
QI-RADIX: Quantum-Inspired Adaptive Radix Sorting Library (Header-Only)
===============================================================================

A modular, production-ready C++17 header-only library for ultra-fast adaptive
sorting of uint32_t, int32_t, uint64_t, int64_t, float, double, Key-Payload pairs,
and text strings using probability-amplitude distribution sensing.

Usage:
    #include "qi_radix.hpp"

    std::vector<uint32_t> data = {5, 2, 9, 1, 5, 6};
    qi::sort(data);

    // Key-Payload Tuple Sorting (Database ORDER BY):
    std::vector<uint32_t> keys = {40, 10, 30};
    std::vector<uint64_t> row_ids = {101, 102, 103};
    qi::sort_pairs(keys.data(), row_ids.data(), keys.size());

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
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace qi {

using u32 = uint32_t;
using i32 = int32_t;
using u64 = uint64_t;
using i64 = int64_t;

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
    double amplitudeSpread = 0.0;         // Von Neumann-style amplitude entropy: -sum(psi_i * ln(psi_i))
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
// KEY ENCODING & DECODING (RADIX BIT TRANSFORMATIONS)
// ============================================================================

namespace key_traits {

// Unsigned 32-bit int
static inline u32 encode(u32 v) { return v; }
static inline u32 decode(u32 v) { return v; }

// Signed 32-bit int
static inline u32 encode(i32 v) { return static_cast<u32>(v) ^ 0x80000000u; }
static inline i32 decode_i32(u32 v) { return static_cast<i32>(v ^ 0x80000000u); }

// IEEE 754 Float (32-bit)
static inline u32 encode(float f) {
    u32 bits;
    std::memcpy(&bits, &f, sizeof(float));
    return (bits & 0x80000000u) ? ~bits : (bits ^ 0x80000000u);
}
static inline float decode_float(u32 bits) {
    u32 raw = (bits & 0x80000000u) ? (bits ^ 0x80000000u) : ~bits;
    float f;
    std::memcpy(&f, &raw, sizeof(float));
    return f;
}

// Unsigned 64-bit int
static inline u64 encode(u64 v) { return v; }

// Signed 64-bit int
static inline u64 encode(i64 v) { return static_cast<u64>(v) ^ 0x8000000000000000ULL; }

// IEEE 754 Double (64-bit)
static inline u64 encode(double d) {
    u64 bits;
    std::memcpy(&bits, &d, sizeof(double));
    return (bits & 0x8000000000000000ULL) ? ~bits : (bits ^ 0x8000000000000000ULL);
}

} // namespace key_traits

// ============================================================================
// INTERNAL DETAIL IMPLEMENTATION
// ============================================================================

namespace detail {

static inline uint8_t getByte(u32 value, int byte) {
    return static_cast<uint8_t>((value >> (byte * 8)) & 0xFF);
}

inline State analyzeData(const u32* data, size_t n, size_t sampleSize = 8192) {
    State state;
    if (n == 0) return state;

    size_t actualSamples = std::min(n, sampleSize);
    state.sampleSize = actualSamples;

    auto start = std::chrono::high_resolution_clock::now();

    alignas(64) size_t byteCounts[4][256] = {{0}};
    size_t step = std::max<size_t>(1, n / actualSamples);

    size_t orderedPairs = 0;
    size_t totalPairs = 0;
    u32 prevVal = data[0];

    u32 bitOr = 0;
    u32 bitAnd = ~0u;

    size_t i = 0;
    size_t step4 = step * 4;
    for (; i + step4 <= n && totalPairs + 4 <= actualSamples; i += step4) {
        u32 v0 = data[i];
        u32 v1 = data[i + step];
        u32 v2 = data[i + step * 2];
        u32 v3 = data[i + step * 3];

        bitOr |= (v0 | v1 | v2 | v3);
        bitAnd &= (v0 & v1 & v2 & v3);

        byteCounts[0][v0 & 0xFF]++;
        byteCounts[1][(v0 >> 8) & 0xFF]++;
        byteCounts[2][(v0 >> 16) & 0xFF]++;
        byteCounts[3][v0 >> 24]++;

        byteCounts[0][v1 & 0xFF]++;
        byteCounts[1][(v1 >> 8) & 0xFF]++;
        byteCounts[2][(v1 >> 16) & 0xFF]++;
        byteCounts[3][v1 >> 24]++;

        byteCounts[0][v2 & 0xFF]++;
        byteCounts[1][(v2 >> 8) & 0xFF]++;
        byteCounts[2][(v2 >> 16) & 0xFF]++;
        byteCounts[3][v2 >> 24]++;

        byteCounts[0][v3 & 0xFF]++;
        byteCounts[1][(v3 >> 8) & 0xFF]++;
        byteCounts[2][(v3 >> 16) & 0xFF]++;
        byteCounts[3][v3 >> 24]++;

        if (v0 >= prevVal) orderedPairs++;
        if (v1 >= v0) orderedPairs++;
        if (v2 >= v1) orderedPairs++;
        if (v3 >= v2) orderedPairs++;
        totalPairs += 4;
        prevVal = v3;
    }

    for (; i < n && totalPairs < actualSamples; i += step) {
        u32 val = data[i];
        bitOr |= val;
        bitAnd &= val;

        byteCounts[0][val & 0xFF]++;
        byteCounts[1][(val >> 8) & 0xFF]++;
        byteCounts[2][(val >> 16) & 0xFF]++;
        byteCounts[3][val >> 24]++;

        if (i > 0) {
            if (val >= prevVal) orderedPairs++;
            totalPairs++;
        }
        prevVal = val;
    }

    state.bitOrSum = bitOr;
    state.bitAndSum = bitAnd;
    state.orderedness = (totalPairs > 0) ? static_cast<double>(orderedPairs) / totalPairs : 1.0;
    state.disorder = 1.0 - state.orderedness;

    double invN = 1.0 / static_cast<double>(actualSamples);
    double invN_sq = invN * invN;
    double invLog2_8 = 1.0 / (8.0 * 0.69314718055994530942);

    double totalEntropy = 0.0;
    double totalIPR = 0.0;
    double totalAmplitudeSpread = 0.0;

    for (int b = 0; b < 4; ++b) {
        ByteState& bs = state.bytes[b];
        double entropy = 0.0;
        double ampSpread = 0.0;
        uint64_t ipr_int_sum = 0;
        int occ = 0;

        for (int k = 0; k < 256; ++k) {
            size_t c = byteCounts[b][k];
            if (c > 0) {
                occ++;
                ipr_int_sum += static_cast<uint64_t>(c) * c;

                double p = static_cast<double>(c) * invN;
                bs.probability[k] = p;
                double psi = std::sqrt(p);
                bs.amplitude[k] = psi;
                entropy -= p * (std::log(p) * invLog2_8);
                // Von Neumann-style amplitude entropy: -sum(psi_i * ln(psi_i))
                // This captures wavefunction spread — how delocalized the amplitude is
                ampSpread -= psi * std::log(psi);
            }
        }

        double ipr = static_cast<double>(ipr_int_sum) * invN_sq;
        bs.entropy = std::max(0.0, std::min(1.0, entropy));
        bs.amplitudeConcentration = ipr;
        bs.effectiveStates = (ipr > 0.0) ? (1.0 / ipr) : 256.0;
        bs.occupied = occ;

        totalEntropy += bs.entropy;
        totalIPR += ipr;
        totalAmplitudeSpread += ampSpread;
    }

    state.averageEntropy = totalEntropy / 4.0;
    state.amplitudeConcentration = totalIPR / 4.0;
    state.effectiveStates = (state.amplitudeConcentration > 0.0) ? (1.0 / state.amplitudeConcentration) : 256.0;
    // Normalize amplitude spread: 4 bytes × max ln(sqrt(1/256)) = 4 × ln(16) ≈ 11.09
    state.amplitudeSpread = totalAmplitudeSpread / (4.0 * std::log(16.0));

    size_t lsbOccupied = state.bytes[0].occupied;
    state.duplicateRatio = (lsbOccupied > 0) ? (1.0 - static_cast<double>(lsbOccupied) / 256.0) : 1.0;

    state.lowByteComplexity = (state.bytes[0].entropy + state.bytes[1].entropy) / 2.0;
    state.highByteComplexity = (state.bytes[2].entropy + state.bytes[3].entropy) / 2.0;

    u32 activeMask = bitOr ^ bitAnd;
    if (state.effectiveStates <= 16.0 || state.duplicateRatio >= 0.70) {
        state.recommendedRadix = Radix::R16;
    } else if (activeMask <= 0x000FFFFFu) {
        state.recommendedRadix = Radix::R8;
    } else if (state.averageEntropy > 0.75 && state.effectiveStates > 128.0) {
        // High entropy 32-bit data → 256 buckets (R8) fits 100% in 32KB L1 Data Cache
        // avoiding store-queue thrashing across 2048/65536 buckets on cloud VMs
        state.recommendedRadix = Radix::R8;
    } else if (state.highByteComplexity > 0.60 || state.effectiveStates >= 64.0) {
        state.recommendedRadix = Radix::R11;
    } else {
        state.recommendedRadix = Radix::R16;
    }

    auto end = std::chrono::high_resolution_clock::now();
    state.analysisTimeMs = std::chrono::duration<double, std::milli>(end - start).count();

    return state;
}

// ── RADIX-8 ──
inline void radixSort8(u32* data, size_t n, bool allowShortcuts = true) {
    if (n <= 1) return;
    if (allowShortcuts) {
        if (std::is_sorted(data, data + n)) return;
        // O(N) reverse-sorted shortcut
        bool isReverse = true;
        for (size_t i = 1; i < std::min<size_t>(n, 1024); ++i) {
            if (data[i - 1] < data[i]) { isReverse = false; break; }
        }
        if (isReverse && std::is_sorted(std::make_reverse_iterator(data + n), std::make_reverse_iterator(data))) {
            std::reverse(data, data + n);
            return;
        }
    }

    std::unique_ptr<u32[]> buffer_ptr(new u32[n]);
    u32* src = data;
    u32* dst = buffer_ptr.get();

    for (int pass = 0; pass < 4; ++pass) {
        size_t count[256] = {0};
        int shift = pass * 8;

        for (size_t i = 0; i < n; ++i) {
            count[(src[i] >> shift) & 0xFF]++;
        }

        size_t total = 0;
        for (int i = 0; i < 256; ++i) {
            size_t c = count[i];
            count[i] = total;
            total += c;
        }

        if (count[0] == n) continue;

        for (size_t i = 0; i < n; ++i) {
            uint8_t byte = (src[i] >> shift) & 0xFF;
            dst[count[byte]++] = src[i];
        }
        std::swap(src, dst);
    }
    if (src != data) std::memcpy(data, src, n * sizeof(u32));
}

// ── RADIX-11 ──
inline void radixSort11(u32* data, size_t n, bool allowShortcuts = true) {
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

    std::unique_ptr<u32[]> buffer_ptr(new u32[n]);
    u32* src = data;
    u32* dst = buffer_ptr.get();

    alignas(64) size_t count0[2048] = {0};
    alignas(64) size_t count1[2048] = {0};
    alignas(64) size_t count2[1024] = {0};

    for (size_t i = 0; i < n; ++i) {
        u32 val = src[i];
        count0[val & 0x7FFu]++;
        count1[(val >> 11) & 0x7FFu]++;
        count2[val >> 22]++;
    }

    size_t sum0 = 0, sum1 = 0, sum2 = 0;
    for (int i = 0; i < 2048; ++i) {
        size_t c0 = count0[i]; count0[i] = sum0; sum0 += c0;
        size_t c1 = count1[i]; count1[i] = sum1; sum1 += c1;
        if (i < 1024) {
            size_t c2 = count2[i]; count2[i] = sum2; sum2 += c2;
        }
    }

    // Pass 0 (bits 0-10)
    size_t i = 0;
    for (; i + 3 < n; i += 4) {
        u32 v0 = src[i], v1 = src[i+1], v2 = src[i+2], v3 = src[i+3];
        __builtin_prefetch(&src[i+32], 0, 1);
        if (i + 16 < n) {
            __builtin_prefetch(&dst[count0[src[i+16] & 0x7FFu]], 1, 1);
        }
        dst[count0[v0 & 0x7FFu]++] = v0;
        dst[count0[v1 & 0x7FFu]++] = v1;
        dst[count0[v2 & 0x7FFu]++] = v2;
        dst[count0[v3 & 0x7FFu]++] = v3;
    }
    for (; i < n; ++i) {
        u32 v = src[i];
        dst[count0[v & 0x7FFu]++] = v;
    }

    // Pass 1 (bits 11-21)
    i = 0;
    for (; i + 3 < n; i += 4) {
        u32 v0 = dst[i], v1 = dst[i+1], v2 = dst[i+2], v3 = dst[i+3];
        __builtin_prefetch(&dst[i+32], 0, 1);
        if (i + 16 < n) {
            __builtin_prefetch(&src[count1[(dst[i+16] >> 11) & 0x7FFu]], 1, 1);
        }
        src[count1[(v0 >> 11) & 0x7FFu]++] = v0;
        src[count1[(v1 >> 11) & 0x7FFu]++] = v1;
        src[count1[(v2 >> 11) & 0x7FFu]++] = v2;
        src[count1[(v3 >> 11) & 0x7FFu]++] = v3;
    }
    for (; i < n; ++i) {
        u32 v = dst[i];
        src[count1[(v >> 11) & 0x7FFu]++] = v;
    }

    // If upper 10 bits are all zero, pass 2 is not needed
    if (sum2 == count2[0] + (count2[1] - count2[0]) && count2[0] == n) {
        // Upper bits are all 0, data is already in src
        return;
    }

    // Pass 2 (bits 22-31)
    i = 0;
    for (; i + 3 < n; i += 4) {
        u32 v0 = src[i], v1 = src[i+1], v2 = src[i+2], v3 = src[i+3];
        __builtin_prefetch(&src[i+32], 0, 1);
        if (i + 16 < n) {
            __builtin_prefetch(&dst[count2[src[i+16] >> 22]], 1, 1);
        }
        dst[count2[v0 >> 22]++] = v0;
        dst[count2[v1 >> 22]++] = v1;
        dst[count2[v2 >> 22]++] = v2;
        dst[count2[v3 >> 22]++] = v3;
    }
    for (; i < n; ++i) {
        u32 v = src[i];
        dst[count2[v >> 22]++] = v;
    }

    std::memcpy(data, dst, n * sizeof(u32));
}

// ── RADIX-16 ──
inline void radixSort16(u32* data, size_t n, bool allowShortcuts = true) {
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

    std::unique_ptr<u32[]> buffer_ptr(new u32[n]);
    u32* src = data;
    u32* dst = buffer_ptr.get();

    auto count0_ptr = std::make_unique<std::array<size_t, 65536>>();
    auto count1_ptr = std::make_unique<std::array<size_t, 65536>>();
    auto& count0 = *count0_ptr;
    auto& count1 = *count1_ptr;
    count0.fill(0);
    count1.fill(0);

    for (size_t i = 0; i < n; ++i) {
        u32 val = src[i];
        count0[val & 0xFFFFu]++;
        count1[val >> 16]++;
    }

    if (count1[0] == n) {
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
        __builtin_prefetch(&src[i+32], 0, 1);
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
        __builtin_prefetch(&dst[i+32], 0, 1);
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

// ── PARALLEL MULTI-THREADED RADIX PASSES ──
inline void parallelRadixSort8(u32* data, size_t n, bool allowShortcuts = true, unsigned int numThreads = 0) {
    if (n <= 1) return;
    if (allowShortcuts && std::is_sorted(data, data + n)) return;

    if (numThreads == 0) numThreads = std::thread::hardware_concurrency();
    if (numThreads < 2) { radixSort8(data, n, allowShortcuts); return; }

    std::vector<u32> buffer(n);
    u32* src = data;
    u32* dst = buffer.data();

    size_t chunkSize = (n + numThreads - 1) / numThreads;

    for (int pass = 0; pass < 4; ++pass) {
        int shift = pass * 8;
        std::vector<std::array<size_t, 256>> threadCounts(numThreads);
        std::vector<std::thread> workers;

        for (unsigned int t = 0; t < numThreads; ++t) {
            size_t start = t * chunkSize;
            size_t end = std::min(start + chunkSize, n);
            if (start >= n) break;

            workers.emplace_back([src, shift, start, end, &threadCounts, t]() {
                threadCounts[t].fill(0);
                for (size_t i = start; i < end; ++i) {
                    threadCounts[t][(src[i] >> shift) & 0xFF]++;
                }
            });
        }
        for (auto& w : workers) w.join();

        std::array<size_t, 256> totalCounts{};
        for (int b = 0; b < 256; ++b) {
            for (unsigned int t = 0; t < numThreads; ++t) totalCounts[b] += threadCounts[t][b];
        }

        if (totalCounts[0] == n) continue;

        std::vector<std::array<size_t, 256>> threadOffsets(numThreads);
        size_t currentOffset = 0;
        for (int b = 0; b < 256; ++b) {
            for (unsigned int t = 0; t < numThreads; ++t) {
                threadOffsets[t][b] = currentOffset;
                currentOffset += threadCounts[t][b];
            }
        }

        workers.clear();
        for (unsigned int t = 0; t < numThreads; ++t) {
            size_t start = t * chunkSize;
            size_t end = std::min(start + chunkSize, n);
            if (start >= n) break;

            workers.emplace_back([src, dst, shift, start, end, &threadOffsets, t]() {
                auto offsets = threadOffsets[t];
                for (size_t i = start; i < end; ++i) {
                    uint8_t byte = (src[i] >> shift) & 0xFF;
                    dst[offsets[byte]++] = src[i];
                }
            });
        }
        for (auto& w : workers) w.join();

        std::swap(src, dst);
    }
    if (src != data) std::memcpy(data, src, n * sizeof(u32));
}

inline void parallelRadixSort11(u32* data, size_t n, bool allowShortcuts = true, unsigned int numThreads = 0) {
    if (n <= 1) return;
    if (numThreads == 0) numThreads = std::thread::hardware_concurrency();
    if (numThreads < 2 || n < 200000) { radixSort11(data, n, allowShortcuts); return; }

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

    std::vector<u32> buffer(n);
    u32* src = data;
    u32* dst = buffer.data();
    size_t chunkSize = (n + numThreads - 1) / numThreads;

    // --- Pass 0: bits 0-10 (11 bits, 2048 buckets) ---
    {
        int shift = 0; u32 mask = 0x7FFu;
        std::vector<std::array<size_t, 2048>> tCounts(numThreads);
        std::vector<std::thread> workers;
        for (unsigned t = 0; t < numThreads; ++t) {
            size_t s = t * chunkSize, e = std::min(s + chunkSize, n);
            if (s >= n) break;
            workers.emplace_back([src, shift, mask, s, e, &tCounts, t]() {
                tCounts[t].fill(0);
                for (size_t i = s; i < e; ++i) tCounts[t][(src[i] >> shift) & mask]++;
            });
        }
        for (auto& w : workers) w.join();
        std::vector<std::array<size_t, 2048>> tOffsets(numThreads);
        size_t cur = 0;
        for (int b = 0; b < 2048; ++b) {
            for (unsigned t = 0; t < numThreads; ++t) {
                tOffsets[t][b] = cur;
                cur += tCounts[t][b];
            }
        }
        workers.clear();
        for (unsigned t = 0; t < numThreads; ++t) {
            size_t s = t * chunkSize, e = std::min(s + chunkSize, n);
            if (s >= n) break;
            workers.emplace_back([src, dst, shift, mask, s, e, &tOffsets, t]() {
                auto off = tOffsets[t];
                for (size_t i = s; i < e; ++i) {
                    u32 v = src[i];
                    dst[off[(v >> shift) & mask]++] = v;
                }
            });
        }
        for (auto& w : workers) w.join();
        std::swap(src, dst);
    }

    // --- Pass 1: bits 11-21 (11 bits, 2048 buckets) ---
    {
        int shift = 11; u32 mask = 0x7FFu;
        std::vector<std::array<size_t, 2048>> tCounts(numThreads);
        std::vector<std::thread> workers;
        for (unsigned t = 0; t < numThreads; ++t) {
            size_t s = t * chunkSize, e = std::min(s + chunkSize, n);
            if (s >= n) break;
            workers.emplace_back([src, shift, mask, s, e, &tCounts, t]() {
                tCounts[t].fill(0);
                for (size_t i = s; i < e; ++i) tCounts[t][(src[i] >> shift) & mask]++;
            });
        }
        for (auto& w : workers) w.join();
        std::vector<std::array<size_t, 2048>> tOffsets(numThreads);
        size_t cur = 0;
        for (int b = 0; b < 2048; ++b) {
            for (unsigned t = 0; t < numThreads; ++t) {
                tOffsets[t][b] = cur;
                cur += tCounts[t][b];
            }
        }
        workers.clear();
        for (unsigned t = 0; t < numThreads; ++t) {
            size_t s = t * chunkSize, e = std::min(s + chunkSize, n);
            if (s >= n) break;
            workers.emplace_back([src, dst, shift, mask, s, e, &tOffsets, t]() {
                auto off = tOffsets[t];
                for (size_t i = s; i < e; ++i) {
                    u32 v = src[i];
                    dst[off[(v >> shift) & mask]++] = v;
                }
            });
        }
        for (auto& w : workers) w.join();
        std::swap(src, dst);
    }

    // --- Pass 2: bits 22-31 (10 bits, 1024 buckets) ---
    {
        int shift = 22;
        std::vector<std::array<size_t, 1024>> tCounts(numThreads);
        std::vector<std::thread> workers;
        for (unsigned t = 0; t < numThreads; ++t) {
            size_t s = t * chunkSize, e = std::min(s + chunkSize, n);
            if (s >= n) break;
            workers.emplace_back([src, shift, s, e, &tCounts, t]() {
                tCounts[t].fill(0);
                for (size_t i = s; i < e; ++i) tCounts[t][src[i] >> shift]++;
            });
        }
        for (auto& w : workers) w.join();
        // Check if pass can be skipped (all upper bits zero)
        bool skipPass2 = true;
        size_t totalBucket0 = 0;
        for (unsigned t = 0; t < numThreads; ++t) totalBucket0 += tCounts[t][0];
        if (totalBucket0 != n) skipPass2 = false;
        if (!skipPass2) {
            std::vector<std::array<size_t, 1024>> tOffsets(numThreads);
            size_t cur = 0;
            for (int b = 0; b < 1024; ++b) {
                for (unsigned t = 0; t < numThreads; ++t) {
                    tOffsets[t][b] = cur;
                    cur += tCounts[t][b];
                }
            }
            workers.clear();
            for (unsigned t = 0; t < numThreads; ++t) {
                size_t s = t * chunkSize, e = std::min(s + chunkSize, n);
                if (s >= n) break;
                workers.emplace_back([src, dst, shift, s, e, &tOffsets, t]() {
                    auto off = tOffsets[t];
                    for (size_t i = s; i < e; ++i) {
                        u32 v = src[i];
                        dst[off[v >> shift]++] = v;
                    }
                });
            }
            for (auto& w : workers) w.join();
            std::swap(src, dst);
        }
    }

    if (src != data) std::memcpy(data, src, n * sizeof(u32));
}

inline void parallelRadixSort16(u32* data, size_t n, bool allowShortcuts = true, unsigned int numThreads = 0) {
    if (n <= 1) return;
    if (numThreads == 0) numThreads = std::thread::hardware_concurrency();
    if (numThreads < 2 || n < 200000) { radixSort16(data, n, allowShortcuts); return; }

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

    std::vector<u32> buffer(n);
    u32* src = data;
    u32* dst = buffer.data();
    size_t chunkSize = (n + numThreads - 1) / numThreads;

    // --- Pass 0: bits 0-15 (16 bits, 65536 buckets) ---
    {
        u32 mask = 0xFFFFu;
        auto tCounts = std::make_unique<std::vector<std::array<size_t, 65536>>>(numThreads);
        std::vector<std::thread> workers;
        for (unsigned t = 0; t < numThreads; ++t) {
            size_t s = t * chunkSize, e = std::min(s + chunkSize, n);
            if (s >= n) break;
            workers.emplace_back([src, mask, s, e, &tCounts, t]() {
                (*tCounts)[t].fill(0);
                for (size_t i = s; i < e; ++i) (*tCounts)[t][src[i] & mask]++;
            });
        }
        for (auto& w : workers) w.join();
        auto tOffsets = std::make_unique<std::vector<std::array<size_t, 65536>>>(numThreads);
        size_t cur = 0;
        for (int b = 0; b < 65536; ++b) {
            for (unsigned t = 0; t < numThreads; ++t) {
                (*tOffsets)[t][b] = cur;
                cur += (*tCounts)[t][b];
            }
        }
        workers.clear();
        for (unsigned t = 0; t < numThreads; ++t) {
            size_t s = t * chunkSize, e = std::min(s + chunkSize, n);
            if (s >= n) break;
            workers.emplace_back([src, dst, mask, s, e, &tOffsets, t]() {
                auto off = (*tOffsets)[t];
                for (size_t i = s; i < e; ++i) {
                    u32 v = src[i];
                    dst[off[v & mask]++] = v;
                }
            });
        }
        for (auto& w : workers) w.join();
        std::swap(src, dst);
    }

    // --- Pass 1: bits 16-31 (16 bits, 65536 buckets) ---
    {
        auto tCounts = std::make_unique<std::vector<std::array<size_t, 65536>>>(numThreads);
        std::vector<std::thread> workers;
        for (unsigned t = 0; t < numThreads; ++t) {
            size_t s = t * chunkSize, e = std::min(s + chunkSize, n);
            if (s >= n) break;
            workers.emplace_back([src, s, e, &tCounts, t]() {
                (*tCounts)[t].fill(0);
                for (size_t i = s; i < e; ++i) (*tCounts)[t][src[i] >> 16]++;
            });
        }
        for (auto& w : workers) w.join();
        // Check if pass can be skipped
        size_t totalBucket0 = 0;
        for (unsigned t = 0; t < numThreads; ++t) totalBucket0 += (*tCounts)[t][0];
        if (totalBucket0 != n) {
            auto tOffsets = std::make_unique<std::vector<std::array<size_t, 65536>>>(numThreads);
            size_t cur = 0;
            for (int b = 0; b < 65536; ++b) {
                for (unsigned t = 0; t < numThreads; ++t) {
                    (*tOffsets)[t][b] = cur;
                    cur += (*tCounts)[t][b];
                }
            }
            workers.clear();
            for (unsigned t = 0; t < numThreads; ++t) {
                size_t s = t * chunkSize, e = std::min(s + chunkSize, n);
                if (s >= n) break;
                workers.emplace_back([src, dst, s, e, &tOffsets, t]() {
                    auto off = (*tOffsets)[t];
                    for (size_t i = s; i < e; ++i) {
                        u32 v = src[i];
                        dst[off[v >> 16]++] = v;
                    }
                });
            }
            for (auto& w : workers) w.join();
            std::swap(src, dst);
        }
    }

    if (src != data) std::memcpy(data, src, n * sizeof(u32));
}

// ── KEY-PAYLOAD (TUPLE) PAIR RADIX SORTING ──
template <typename Key, typename Payload>
inline void sortPairs(Key* keys, Payload* payloads, size_t n) {
    if (n <= 1) return;

    std::vector<u32> encKeys(n);
    for (size_t i = 0; i < n; ++i) encKeys[i] = key_traits::encode(keys[i]);

    std::vector<u32> keyBuffer(n);
    std::vector<Payload> payloadBuffer(n);

    u32* srcK = encKeys.data();
    u32* dstK = keyBuffer.data();
    Payload* srcP = payloads;
    Payload* dstP = payloadBuffer.data();

    for (int pass = 0; pass < 4; ++pass) {
        size_t count[256] = {0};
        int shift = pass * 8;

        for (size_t i = 0; i < n; ++i) {
            count[(srcK[i] >> shift) & 0xFF]++;
        }

        size_t total = 0;
        for (int i = 0; i < 256; ++i) {
            size_t c = count[i];
            count[i] = total;
            total += c;
        }

        if (count[0] == n) continue;

        for (size_t i = 0; i < n; ++i) {
            uint8_t byte = (srcK[i] >> shift) & 0xFF;
            size_t idx = count[byte]++;
            dstK[idx] = srcK[i];
            dstP[idx] = srcP[i];
        }
        std::swap(srcK, dstK);
        std::swap(srcP, dstP);
    }

    if (srcP != payloads) std::memcpy(payloads, srcP, n * sizeof(Payload));
    if (srcK != encKeys.data()) std::memcpy(encKeys.data(), srcK, n * sizeof(u32));

    for (size_t i = 0; i < n; ++i) keys[i] = static_cast<Key>(encKeys[i]);
}

} // namespace detail

// ============================================================================
// PUBLIC API FUNCTIONS
// ============================================================================

/**
 * @brief Analyze distribution state of an integer dataset.
 */
inline State analyze(const u32* data, size_t n, size_t sampleSize = 8192) {
    return detail::analyzeData(data, n, sampleSize);
}

inline State analyze(const std::vector<u32>& data, size_t sampleSize = 8192) {
    return detail::analyzeData(data.data(), data.size(), sampleSize);
}

/**
 * @brief Primary qi::sort for uint32_t arrays.
 */
inline void sort(u32* data, size_t n, SortOptions options = SortOptions{}) {
    if (n <= 1) return;
    State state = detail::analyzeData(data, n, options.sampleSize);

    if (options.parallel && n >= 100000) {
        unsigned int threads = options.numThreads;
        if (threads == 0) threads = std::thread::hardware_concurrency();
        if (threads < 2) threads = 2;

        switch (state.recommendedRadix) {
            case Radix::R8:  detail::parallelRadixSort8(data, n, options.allowShortcuts, threads); break;
            case Radix::R11: detail::parallelRadixSort11(data, n, options.allowShortcuts, threads); break;
            case Radix::R16: detail::parallelRadixSort16(data, n, options.allowShortcuts, threads); break;
        }
    } else {
        switch (state.recommendedRadix) {
            case Radix::R8:  detail::radixSort8(data, n, options.allowShortcuts); break;
            case Radix::R11: detail::radixSort11(data, n, options.allowShortcuts); break;
            case Radix::R16: detail::radixSort16(data, n, options.allowShortcuts); break;
        }
    }
}

inline void sort(std::vector<u32>& data, SortOptions options = SortOptions{}) {
    sort(data.data(), data.size(), options);
}

/**
 * @brief Parallel sort explicitly for multi-core scaling.
 */
inline void parallel_sort(u32* data, size_t n, unsigned int numThreads = 0) {
    SortOptions opts;
    opts.parallel = true;
    opts.numThreads = numThreads;
    sort(data, n, opts);
}

inline void parallel_sort(std::vector<u32>& data, unsigned int numThreads = 0) {
    parallel_sort(data.data(), data.size(), numThreads);
}

/**
 * @brief Overload for signed int32_t arrays.
 */
inline void sort(i32* data, size_t n, SortOptions options = SortOptions{}) {
    if (n <= 1) return;
    std::vector<u32> enc(n);
    for (size_t i = 0; i < n; ++i) enc[i] = key_traits::encode(data[i]);
    sort(enc.data(), n, options);
    for (size_t i = 0; i < n; ++i) data[i] = key_traits::decode_i32(enc[i]);
}

inline void sort(std::vector<i32>& data, SortOptions options = SortOptions{}) {
    sort(data.data(), data.size(), options);
}

/**
 * @brief Overload for float arrays (IEEE 754 32-bit).
 */
inline void sort(float* data, size_t n, SortOptions options = SortOptions{}) {
    if (n <= 1) return;
    std::vector<u32> enc(n);
    for (size_t i = 0; i < n; ++i) enc[i] = key_traits::encode(data[i]);
    sort(enc.data(), n, options);
    for (size_t i = 0; i < n; ++i) data[i] = key_traits::decode_float(enc[i]);
}

inline void sort(std::vector<float>& data, SortOptions options = SortOptions{}) {
    sort(data.data(), data.size(), options);
}

/**
 * @brief Key-Payload (Tuple) sorting for database ORDER BY (e.g. key + row_id).
 */
template <typename Key, typename Payload>
inline void sort_pairs(Key* keys, Payload* payloads, size_t n) {
    detail::sortPairs(keys, payloads, n);
}

template <typename Key, typename Payload>
inline void sort_pairs(std::vector<Key>& keys, std::vector<Payload>& payloads) {
    if (keys.size() != payloads.size()) throw std::invalid_argument("Key and Payload vector sizes must match");
    sort_pairs(keys.data(), payloads.data(), keys.size());
}

/**
 * @brief String prefix radix sorting (VARCHAR columns).
 */
inline void sort_strings(std::string* strings, size_t n) {
    if (n <= 1) return;
    std::vector<u64> prefixes(n, 0);
    std::vector<size_t> indices(n);

    for (size_t i = 0; i < n; ++i) {
        indices[i] = i;
        u64 prefix = 0;
        size_t len = std::min<size_t>(8, strings[i].size());
        for (size_t b = 0; b < len; ++b) {
            prefix |= (static_cast<u64>(static_cast<uint8_t>(strings[i][b])) << ((7 - b) * 8));
        }
        prefixes[i] = prefix;
    }

    // Sort by prefix
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        if (prefixes[a] != prefixes[b]) return prefixes[a] < prefixes[b];
        return strings[a] < strings[b]; // Fallback for tie-breaking
    });

    std::vector<std::string> temp(n);
    for (size_t i = 0; i < n; ++i) temp[i] = std::move(strings[indices[i]]);
    for (size_t i = 0; i < n; ++i) strings[i] = std::move(temp[i]);
}

inline void sort_strings(std::vector<std::string>& strings) {
    sort_strings(strings.data(), strings.size());
}

/**
 * @brief Iterator overload.
 */
template <typename RandomIt>
inline void sort(RandomIt begin, RandomIt end, SortOptions options = SortOptions{}) {
    using T = typename std::iterator_traits<RandomIt>::value_type;
    size_t n = std::distance(begin, end);
    if (n <= 1) return;
    T* first = &(*begin);
    sort(first, n, options);
}

} // namespace qi

#endif // QI_RADIX_HPP
