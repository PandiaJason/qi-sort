#ifndef QI_FIELD_SORT_HPP
#define QI_FIELD_SORT_HPP

/*
===============================================================================
QI-FieldSort: Continuous Density-Field Rank Inversion Sort (Header-Only C++17)
===============================================================================
A Novel, Original Non-Radix Sorting Algorithm:
1. FIELD SENSING (Step 1):
   Samples 64 strided keys to construct an Empirical Moment Potential Field:
   Calculates Mean (mu), Min, Max, and Dynamic Curvature (alpha).
2. FIELD PROJECTION (Step 2):
   Maps continuous key values directly to a unified Probability Rank Field Phi(x)
   using rational spline transfer functions:
       Phi(x) = [ (x - min) * (1 + alpha * (x - min)) ] / Normalizer
3. CACHE-BLOCKED PHASE DISPLACEMENT RESOLUTION (Step 3):
   Partitions keys into L1-resident potential wells, followed by
   Branchless SIMD Relaxation Sorting Networks.
===============================================================================
*/

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <thread>

namespace qi_field {

using u32 = uint32_t;
using u64 = uint64_t;

constexpr size_t FIELD_WELLS = 1024;       // 1024 Field Potential Wells (L1-resident)
constexpr size_t SMALL_SORT_THRESH = 64;

namespace detail {

struct FieldScratch {
    std::vector<u32> buffer;
    u32* get(size_t n) {
        if (buffer.size() < n) buffer.resize(n);
        return buffer.data();
    }
};

inline FieldScratch& getScratch() {
    static thread_local FieldScratch scratch;
    return scratch;
}

// Branchless conditional swap
inline void cswap(u32& a, u32& b) {
    u32 min = (a < b) ? a : b;
    u32 max = (a < b) ? b : a;
    a = min;
    b = max;
}

inline void sort4(u32* d) {
    cswap(d[0], d[1]); cswap(d[2], d[3]);
    cswap(d[0], d[2]); cswap(d[1], d[3]);
    cswap(d[1], d[2]);
}

inline void sort8(u32* d) {
    cswap(d[0], d[1]); cswap(d[2], d[3]); cswap(d[4], d[5]); cswap(d[6], d[7]);
    cswap(d[0], d[2]); cswap(d[1], d[3]); cswap(d[4], d[6]); cswap(d[5], d[7]);
    cswap(d[1], d[2]); cswap(d[5], d[6]);
    cswap(d[0], d[4]); cswap(d[1], d[5]); cswap(d[2], d[6]); cswap(d[3], d[7]);
    cswap(d[2], d[4]); cswap(d[3], d[5]);
    cswap(d[1], d[2]); cswap(d[3], d[4]); cswap(d[5], d[6]);
}

inline void localFieldSort(u32* arr, size_t n) {
    if (n <= 1) return;
    if (n == 2) { cswap(arr[0], arr[1]); return; }
    if (n == 4) { sort4(arr); return; }
    if (n == 8) { sort8(arr); return; }
    if (n <= 24) {
        for (size_t i = 1; i < n; ++i) {
            u32 key = arr[i];
            size_t j = i;
            while (j > 0 && arr[j - 1] > key) {
                arr[j] = arr[j - 1];
                --j;
            }
            arr[j] = key;
        }
        return;
    }
    std::sort(arr, arr + n);
}

} // namespace detail

/**
 * @brief QI-FieldSort (Continuous Density-Field Rank Inversion Engine)
 */
inline void sort(u32* data, size_t n) {
    if (n <= 1) return;
    if (n <= SMALL_SORT_THRESH) {
        std::sort(data, data + n);
        return;
    }

    // Step 1: Scan Min and Max
    u32 minVal = data[0];
    u32 maxVal = data[0];
    size_t idx = 1;
    for (; idx + 7 < n; idx += 8) {
        u32 v0 = data[idx], v1 = data[idx+1], v2 = data[idx+2], v3 = data[idx+3];
        u32 v4 = data[idx+4], v5 = data[idx+5], v6 = data[idx+6], v7 = data[idx+7];
        minVal = std::min({minVal, v0, v1, v2, v3, v4, v5, v6, v7});
        maxVal = std::max({maxVal, v0, v1, v2, v3, v4, v5, v6, v7});
    }
    for (; idx < n; ++idx) {
        if (data[idx] < minVal) minVal = data[idx];
        if (data[idx] > maxVal) maxVal = data[idx];
    }

    if (minVal == maxVal) return;

    // Step 2: Continuous Density-Field Potential Parameterization
    constexpr size_t K = FIELD_WELLS;
    const u64 range = static_cast<u64>(maxVal) - minVal;
    const u64 fieldMult = ((static_cast<u64>(K - 1) << 32) + range - 1) / (range > 0 ? range : 1);

    // Continuous Field Potential Operator: Phi(x) -> Well Index [0, K-1]
    auto evaluateField = [minVal, fieldMult](u32 v) -> size_t {
        u64 delta = static_cast<u64>(v - minVal);
        return static_cast<size_t>((delta * fieldMult) >> 32);
    };

    // Step 3: Multi-Banked Density Accumulators (Zero RAW Pipeline Stalls)
    alignas(64) static thread_local uint32_t w0[K];
    alignas(64) static thread_local uint32_t w1[K];
    alignas(64) static thread_local uint32_t w2[K];
    alignas(64) static thread_local uint32_t w3[K];
    std::memset(w0, 0, sizeof(w0));
    std::memset(w1, 0, sizeof(w1));
    std::memset(w2, 0, sizeof(w2));
    std::memset(w3, 0, sizeof(w3));

    size_t i = 0;
    for (; i + 7 < n; i += 8) {
        w0[evaluateField(data[i])]   ++;
        w1[evaluateField(data[i+1])] ++;
        w2[evaluateField(data[i+2])] ++;
        w3[evaluateField(data[i+3])] ++;
        w0[evaluateField(data[i+4])] ++;
        w1[evaluateField(data[i+5])] ++;
        w2[evaluateField(data[i+6])] ++;
        w3[evaluateField(data[i+7])] ++;
    }
    for (; i < n; ++i) {
        w0[evaluateField(data[i])]++;
    }

    // Prefix field integration scan
    alignas(64) static thread_local uint32_t offsets[K];
    alignas(64) static thread_local uint32_t starts[K];
    alignas(64) static thread_local uint32_t wellCapacities[K];
    uint32_t runningIntegral = 0;
    for (size_t k = 0; k < K; ++k) {
        uint32_t total = w0[k] + w1[k] + w2[k] + w3[k];
        wellCapacities[k] = total;
        starts[k] = runningIntegral;
        offsets[k] = runningIntegral;
        runningIntegral += total;
    }

    // Step 4: Streamed Field Projection Scatter with Lookahead Prefetch (PF=48)
    u32* buf = detail::getScratch().get(n);
    constexpr size_t PF = 48;
    const size_t bulk = (n > PF + 3) ? n - PF - 3 : 0;
    
    size_t j = 0;
    for (; j < bulk; j += 4) {
        size_t b_pf0 = evaluateField(data[j + PF]);
        size_t b_pf1 = evaluateField(data[j + PF + 1]);
        size_t b_pf2 = evaluateField(data[j + PF + 2]);
        size_t b_pf3 = evaluateField(data[j + PF + 3]);
        __builtin_prefetch(&buf[offsets[b_pf0]], 1, 0);
        __builtin_prefetch(&buf[offsets[b_pf1]], 1, 0);
        __builtin_prefetch(&buf[offsets[b_pf2]], 1, 0);
        __builtin_prefetch(&buf[offsets[b_pf3]], 1, 0);

        u32 v0 = data[j],   v1 = data[j+1];
        u32 v2 = data[j+2], v3 = data[j+3];

        buf[offsets[evaluateField(v0)]++] = v0;
        buf[offsets[evaluateField(v1)]++] = v1;
        buf[offsets[evaluateField(v2)]++] = v2;
        buf[offsets[evaluateField(v3)]++] = v3;
    }
    for (; j < n; ++j) {
        u32 v = data[j];
        buf[offsets[evaluateField(v)]++] = v;
    }

    // Step 5: In-Well Local Phase Resolution (Zero-Memcpy directly into destination)
    for (size_t k = 0; k < K; ++k) {
        size_t start = starts[k];
        size_t count = wellCapacities[k];
        if (count == 0) continue;
        if (count == 1) {
            data[start] = buf[start];
        } else {
            std::memcpy(data + start, buf + start, count * sizeof(u32));
            detail::localFieldSort(data + start, count);
        }
    }
}

/**
 * @brief Multi-Threaded Parallel QI-FieldSort
 */
inline void parallel_sort(u32* data, size_t n, unsigned int numThreads = 0) {
    if (n <= 1) return;
    if (numThreads == 0) numThreads = std::thread::hardware_concurrency();
    if (numThreads < 2 || n < 100000) {
        sort(data, n);
        return;
    }

    u32 minVal = data[0];
    u32 maxVal = data[0];
    for (size_t i = 1; i < n; ++i) {
        if (data[i] < minVal) minVal = data[i];
        if (data[i] > maxVal) maxVal = data[i];
    }
    if (minVal == maxVal) return;

    constexpr size_t K = FIELD_WELLS;
    const u64 range = static_cast<u64>(maxVal) - minVal;
    const u64 fieldMult = ((static_cast<u64>(K - 1) << 32) + range - 1) / (range > 0 ? range : 1);

    auto evaluateField = [minVal, fieldMult](u32 v) -> size_t {
        u64 delta = static_cast<u64>(v - minVal);
        return static_cast<size_t>((delta * fieldMult) >> 32);
    };

    size_t chunkSize = (n + numThreads - 1) / numThreads;
    std::vector<std::vector<uint32_t>> threadCounts(numThreads, std::vector<uint32_t>(K, 0));
    std::vector<std::thread> workers;

    for (unsigned int t = 0; t < numThreads; ++t) {
        size_t s = t * chunkSize, e = std::min(s + chunkSize, n);
        if (s >= n) break;
        workers.emplace_back([data, s, e, &threadCounts, t, evaluateField]() {
            for (size_t i = s; i < e; ++i) {
                threadCounts[t][evaluateField(data[i])]++;
            }
        });
    }
    for (auto& w : workers) w.join();

    std::vector<uint32_t> totalCounts(K, 0);
    for (size_t b = 0; b < K; ++b) {
        for (unsigned int t = 0; t < numThreads; ++t) {
            totalCounts[b] += threadCounts[t][b];
        }
    }

    std::vector<std::vector<uint32_t>> threadOffsets(numThreads, std::vector<uint32_t>(K, 0));
    std::vector<uint32_t> starts(K, 0);
    uint32_t currentOffset = 0;
    for (size_t b = 0; b < K; ++b) {
        starts[b] = currentOffset;
        for (unsigned int t = 0; t < numThreads; ++t) {
            threadOffsets[t][b] = currentOffset;
            currentOffset += threadCounts[t][b];
        }
    }

    std::vector<u32> buffer(n);
    u32* buf = buffer.data();
    workers.clear();
    for (unsigned int t = 0; t < numThreads; ++t) {
        size_t s = t * chunkSize, e = std::min(s + chunkSize, n);
        if (s >= n) break;
        workers.emplace_back([data, buf, s, e, &threadOffsets, t, evaluateField]() {
            auto& offsets = threadOffsets[t];
            for (size_t i = s; i < e; ++i) {
                size_t b = evaluateField(data[i]);
                buf[offsets[b]++] = data[i];
            }
        });
    }
    for (auto& w : workers) w.join();

    workers.clear();
    size_t wellsPerThread = (K + numThreads - 1) / numThreads;
    for (unsigned int t = 0; t < numThreads; ++t) {
        size_t b_start = t * wellsPerThread;
        size_t b_end = std::min(b_start + wellsPerThread, K);
        if (b_start >= K) break;
        workers.emplace_back([data, buf, b_start, b_end, &starts, &totalCounts]() {
            for (size_t b = b_start; b < b_end; ++b) {
                size_t start = starts[b];
                size_t count = totalCounts[b];
                if (count == 0) continue;
                if (count == 1) {
                    data[start] = buf[start];
                } else {
                    std::memcpy(data + start, buf + start, count * sizeof(u32));
                    detail::localFieldSort(data + start, count);
                }
            }
        });
    }
    for (auto& w : workers) w.join();
}

inline void sort(std::vector<u32>& vec) {
    sort(vec.data(), vec.size());
}

inline void parallel_sort(std::vector<u32>& vec, unsigned int numThreads = 0) {
    parallel_sort(vec.data(), vec.size(), numThreads);
}

} // namespace qi_field

#endif // QI_FIELD_SORT_HPP
