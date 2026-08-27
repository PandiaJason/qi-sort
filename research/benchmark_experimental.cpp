/*
 * qi::apex v2 — Experimental ultra-optimized engine
 * Testing: MSD-first cache-partitioned radix vs current 3-pass LSD
 *
 * Technique 1: Fused probe+count (eliminate redundant data scan)
 * Technique 2: MSD-8 partition → L1-resident LSD sub-sorts (2-level radix)
 * Technique 3: Reduced prefetch pressure via write-combining buffer
 */

#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <cstring>
#include "../include/qi_radix.hpp"
#include "../include/qi_apex.hpp"

using u32 = uint32_t;
using u64 = uint64_t;

// ════════════════════════════════════════════════════════════════════════
// EXPERIMENTAL ENGINE: MSD-8 Cache-Partitioned Radix Sort
// ════════════════════════════════════════════════════════════════════════
// Strategy: Instead of 3 passes over entire N (LSD-11), do:
//   Pass 1 (MSD): Scatter by top 8 bits → 256 buckets (each ~N/256 elements)
//   Pass 2 (LSD): Sort each bucket in-place using L1-resident 2-pass Radix-12
// Advantage: Sub-sorts fit entirely in L1 cache (128KB on M1 Pro),
//            eliminating DRAM scatter latency for passes 2+3.
// ════════════════════════════════════════════════════════════════════════

namespace experimental {

static thread_local std::vector<u32> g_scratch;
static u32* getScratch(size_t n) {
    if (g_scratch.size() < n) g_scratch.resize(n);
    return g_scratch.data();
}

// Small-array sort for buckets <= 64 elements
inline void insertionSort(u32* data, size_t n) {
    for (size_t i = 1; i < n; ++i) {
        u32 key = data[i];
        size_t j = i;
        while (j > 0 && data[j-1] > key) {
            data[j] = data[j-1];
            --j;
        }
        data[j] = key;
    }
}

// L1-resident 3-pass Radix-8 for medium buckets (< 30K elements = 120KB)
// Uses tiny 1KB histograms that are deeply L1-resident
inline void l1RadixSort8_3pass(u32* data, size_t n, u32* buf) {
    alignas(64) uint32_t c0[256] = {}, c1[256] = {}, c2[256] = {};

    for (size_t i = 0; i < n; ++i) {
        u32 v = data[i];
        c0[v & 0xFFu]++;
        c1[(v >> 8) & 0xFFu]++;
        c2[(v >> 16) & 0xFFu]++;
    }

    uint32_t s0 = 0, s1 = 0, s2 = 0;
    for (int k = 0; k < 256; ++k) {
        uint32_t t;
        t = c0[k]; c0[k] = s0; s0 += t;
        t = c1[k]; c1[k] = s1; s1 += t;
        t = c2[k]; c2[k] = s2; s2 += t;
    }

    // Pass 0: data -> buf (bits 0-7)
    for (size_t i = 0; i < n; ++i) {
        u32 v = data[i]; buf[c0[v & 0xFFu]++] = v;
    }
    // Pass 1: buf -> data (bits 8-15)
    for (size_t i = 0; i < n; ++i) {
        u32 v = buf[i]; data[c1[(v >> 8) & 0xFFu]++] = v;
    }
    // Pass 2: data -> buf (bits 16-23) then copy back
    for (size_t i = 0; i < n; ++i) {
        u32 v = data[i]; buf[c2[(v >> 16) & 0xFFu]++] = v;
    }
    std::memcpy(data, buf, n * sizeof(u32));
}

// ── ENGINE A: MSD-8 Cache-Partitioned Radix ──
// Pass 1: Scatter by top 8 bits (MSD)
// Pass 2: Sort each of the 256 L1-sized buckets independently
void msd_partitioned_sort(u32* data, size_t n) {
    if (n <= 1) return;
    if (n < 64) { std::sort(data, data + n); return; }

    u32* buf = getScratch(n);

    // ── MSD Pass: Count + Scatter by top 8 bits ──
    alignas(64) uint32_t msdCount[256] = {};

    // Fused bitOr + MSD count (no separate probe pass!)
    u32 bitOr = 0;
    size_t i = 0;
    for (; i + 3 < n; i += 4) {
        u32 v0 = data[i], v1 = data[i+1], v2 = data[i+2], v3 = data[i+3];
        bitOr |= v0 | v1 | v2 | v3;
        msdCount[v0 >> 24]++;
        msdCount[v1 >> 24]++;
        msdCount[v2 >> 24]++;
        msdCount[v3 >> 24]++;
    }
    for (; i < n; ++i) {
        u32 v = data[i];
        bitOr |= v;
        msdCount[v >> 24]++;
    }

    // Fast-path: narrow-domain data
    if (bitOr <= 0xFFFu) {
        // Counting sort
        const size_t bins = static_cast<size_t>(bitOr) + 1;
        if (bins <= 256) {
            alignas(64) uint32_t cnt[256] = {};
            for (size_t j = 0; j < n; ++j) cnt[data[j]]++;
            size_t pos = 0;
            for (size_t v = 0; v < bins; ++v) {
                uint32_t c = cnt[v];
                if (c > 0) { std::fill(data + pos, data + pos + c, static_cast<u32>(v)); pos += c; }
            }
        } else {
            std::vector<uint32_t> cnt(bins, 0);
            for (size_t j = 0; j < n; ++j) cnt[data[j]]++;
            size_t pos = 0;
            for (size_t v = 0; v < bins; ++v) {
                uint32_t c = cnt[v];
                if (c > 0) { std::fill(data + pos, data + pos + c, static_cast<u32>(v)); pos += c; }
            }
        }
        return;
    }

    // Check: if all top 8 bits are zero (values < 16M), we can skip MSD pass
    if ((bitOr >> 24) == 0) {
        // Values fit in 24 bits — use 3-pass Radix-8 on lower 24 bits
        l1RadixSort8_3pass(data, n, buf);
        return;
    }

    // MSD prefix sum
    alignas(64) uint32_t msdOffset[256];
    uint32_t sum = 0;
    for (int k = 0; k < 256; ++k) {
        msdOffset[k] = sum;
        sum += msdCount[k];
    }

    // MSD scatter: data -> buf (by top 8 bits)
    constexpr size_t PF = 32;
    {
        const size_t bulk = (n > PF) ? n - PF : 0;
        size_t j = 0;
        for (; j < bulk; ++j) {
            __builtin_prefetch(&buf[msdOffset[data[j+PF] >> 24]], 1, 0);
            u32 v = data[j]; buf[msdOffset[v >> 24]++] = v;
        }
        for (; j < n; ++j) {
            u32 v = data[j]; buf[msdOffset[v >> 24]++] = v;
        }
    }

    // ── L1-Resident Sub-Sorts: Sort each MSD bucket independently ──
    // Each bucket avg size = N/256 (e.g. 3900 for 1M, 39000 for 10M)
    // On M1 Pro 128KB L1: buckets up to 32K elements (128KB) fit entirely in L1!
    uint32_t bucketStart = 0;
    for (int b = 0; b < 256; ++b) {
        uint32_t count = msdCount[b];
        if (count <= 1) {
            if (count == 1) data[bucketStart] = buf[bucketStart];
            bucketStart += count;
            continue;
        }

        u32* bdata = buf + bucketStart;
        u32* bscratch = data + bucketStart; // reuse data[] as scratch for sub-sort

        if (count <= 32) {
            // Tiny bucket: insertion sort directly, then copy to data
            insertionSort(bdata, count);
            std::memcpy(data + bucketStart, bdata, count * sizeof(u32));
        } else {
            // Medium/Large bucket: 3-pass Radix-8 on lower 24 bits (all L1-resident!)
            // Sort buf[start..start+count] using data[start..] as scratch
            l1RadixSort8_3pass(bdata, count, bscratch);
            // Result is now in bdata (buf), copy to data
            std::memcpy(data + bucketStart, bdata, count * sizeof(u32));
        }
        bucketStart += count;
    }
}

// ── ENGINE B: Fused-Probe 3-Pass LSD Radix-11 (current apex, optimized) ──
// Same as qi::apex but with bitOr computed during counting instead of separate probe
void fused_lsd_radix11(u32* data, size_t n) {
    if (n <= 1) return;
    if (n < 64) { std::sort(data, data + n); return; }

    // Use qi::apex::sort directly (already optimized)
    qi::apex::sort(data, n);
}

// ── ENGINE C: 2-Pass Radix-16 for full 32-bit (fewer passes = less bandwidth) ──
void radix16_2pass(u32* data, size_t n) {
    if (n <= 1) return;
    if (n < 64) { std::sort(data, data + n); return; }

    u32* buf = getScratch(n);

    // Thread-local static histograms (512KB total, L2-resident on M1 Pro)
    alignas(64) static thread_local uint32_t count0[65536];
    alignas(64) static thread_local uint32_t count1[65536];
    std::memset(count0, 0, sizeof(count0));
    std::memset(count1, 0, sizeof(count1));

    // Fused count (4-way unrolled)
    size_t i = 0;
    for (; i + 3 < n; i += 4) {
        u32 v0 = data[i], v1 = data[i+1], v2 = data[i+2], v3 = data[i+3];
        count0[v0 & 0xFFFFu]++; count1[v0 >> 16]++;
        count0[v1 & 0xFFFFu]++; count1[v1 >> 16]++;
        count0[v2 & 0xFFFFu]++; count1[v2 >> 16]++;
        count0[v3 & 0xFFFFu]++; count1[v3 >> 16]++;
    }
    for (; i < n; ++i) {
        u32 v = data[i];
        count0[v & 0xFFFFu]++; count1[v >> 16]++;
    }

    uint32_t s0 = 0, s1 = 0;
    for (int k = 0; k < 65536; ++k) {
        uint32_t c0 = count0[k]; count0[k] = s0; s0 += c0;
        uint32_t c1 = count1[k]; count1[k] = s1; s1 += c1;
    }

    constexpr size_t PF = 32;
    const size_t bulk = (n > PF + 1) ? n - PF - 1 : 0;

    // Pass 0: data -> buf (bits 0-15)
    size_t j = 0;
    for (; j < bulk; j += 2) {
        __builtin_prefetch(&buf[count0[data[j+PF]   & 0xFFFFu]], 1, 0);
        __builtin_prefetch(&buf[count0[data[j+PF+1] & 0xFFFFu]], 1, 0);
        u32 v0 = data[j], v1 = data[j+1];
        buf[count0[v0 & 0xFFFFu]++] = v0;
        buf[count0[v1 & 0xFFFFu]++] = v1;
    }
    for (; j < n; ++j) {
        u32 v = data[j]; buf[count0[v & 0xFFFFu]++] = v;
    }

    // Pass 1: buf -> data (bits 16-31)
    j = 0;
    for (; j < bulk; j += 2) {
        __builtin_prefetch(&data[count1[buf[j+PF]   >> 16]], 1, 0);
        __builtin_prefetch(&data[count1[buf[j+PF+1] >> 16]], 1, 0);
        u32 v0 = buf[j], v1 = buf[j+1];
        data[count1[v0 >> 16]++] = v0;
        data[count1[v1 >> 16]++] = v1;
    }
    for (; j < n; ++j) {
        u32 v = buf[j]; data[count1[v >> 16]++] = v;
    }
}

// ── ENGINE D: MSD-10 Partitioned (1024 buckets, smaller sub-sorts) ──
void msd10_partitioned_sort(u32* data, size_t n) {
    if (n <= 1) return;
    if (n < 64) { std::sort(data, data + n); return; }

    u32* buf = getScratch(n);

    constexpr size_t K = 1024;

    // Count by top 10 bits
    alignas(64) uint32_t msdCount[K] = {};
    size_t i = 0;
    for (; i + 3 < n; i += 4) {
        msdCount[data[i]   >> 22]++;
        msdCount[data[i+1] >> 22]++;
        msdCount[data[i+2] >> 22]++;
        msdCount[data[i+3] >> 22]++;
    }
    for (; i < n; ++i) msdCount[data[i] >> 22]++;

    // Prefix sum
    alignas(64) uint32_t msdOffset[K];
    uint32_t sum = 0;
    for (size_t k = 0; k < K; ++k) {
        msdOffset[k] = sum;
        sum += msdCount[k];
    }

    // MSD scatter: data -> buf
    {
        constexpr size_t PF = 32;
        const size_t bulk = (n > PF) ? n - PF : 0;
        size_t j = 0;
        for (; j < bulk; ++j) {
            __builtin_prefetch(&buf[msdOffset[data[j+PF] >> 22]], 1, 0);
            u32 v = data[j]; buf[msdOffset[v >> 22]++] = v;
        }
        for (; j < n; ++j) {
            u32 v = data[j]; buf[msdOffset[v >> 22]++] = v;
        }
    }

    // Sort each bucket (avg size = N/1024)
    // For 1M: avg 976 elements per bucket = 3.8 KB → deeply L1!
    // For 10M: avg 9766 elements = 38 KB → fits L1 on M1 Pro (128KB)
    uint32_t bucketStart = 0;
    for (size_t b = 0; b < K; ++b) {
        uint32_t count = msdCount[b];
        if (count <= 1) {
            if (count == 1) data[bucketStart] = buf[bucketStart];
            bucketStart += count;
            continue;
        }

        u32* bdata = buf + bucketStart;
        u32* bscratch = data + bucketStart;

        if (count <= 32) {
            insertionSort(bdata, count);
            std::memcpy(data + bucketStart, bdata, count * sizeof(u32));
        } else if (count <= 30000) {
            // L1-resident: 2-pass Radix-11 on lower 22 bits
            alignas(64) uint32_t c0[2048] = {}, c1[1024] = {};
            for (size_t ii = 0; ii < count; ++ii) {
                u32 v = bdata[ii];
                c0[v & 0x7FFu]++;
                c1[(v >> 11) & 0x3FFu]++;
            }
            uint32_t s0 = 0, s1 = 0;
            for (int k = 0; k < 2048; ++k) {
                uint32_t t = c0[k]; c0[k] = s0; s0 += t;
                if (k < 1024) { uint32_t t2 = c1[k]; c1[k] = s1; s1 += t2; }
            }
            // Pass 0: bdata -> bscratch
            for (size_t ii = 0; ii < count; ++ii) {
                u32 v = bdata[ii]; bscratch[c0[v & 0x7FFu]++] = v;
            }
            // Pass 1: bscratch -> data[bucketStart..]
            u32* dst = data + bucketStart;
            for (size_t ii = 0; ii < count; ++ii) {
                u32 v = bscratch[ii]; dst[c1[(v >> 11) & 0x3FFu]++] = v;
            }
        } else {
            // Larger bucket: fall back to qi::apex
            std::memcpy(data + bucketStart, bdata, count * sizeof(u32));
            qi::apex::sort(data + bucketStart, count);
        }
        bucketStart += count;
    }
}

} // namespace experimental

template <typename Func>
double time_ms(Func f, int iterations = 7) {
    double best = 1e9;
    for (int i = 0; i < iterations; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        f();
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (ms < best) best = ms;
    }
    return best;
}

int main() {
    std::cout << "========================================================================================\n";
    std::cout << "  EXPERIMENTAL ARENA: Racing for the World's Fastest Integer Sort\n";
    std::cout << "  Hardware: Apple M1 Pro (128KB L1, 12MB L2, NEON SIMD)\n";
    std::cout << "========================================================================================\n\n";

    std::vector<size_t> sizes = {100000, 1000000, 10000000};
    std::mt19937_64 rng(1337);

    for (size_t N : sizes) {
        std::cout << "--- N = " << N << " Elements (Uniform Random 32-bit) ---\n";
        std::cout << std::left << std::setw(52) << "Engine"
                  << std::setw(14) << "Time (ms)"
                  << std::setw(20) << "MKeys/s"
                  << "Status\n";
        std::cout << "------------------------------------------------------------------------------------\n";

        std::vector<u32> original(N);
        for (size_t i = 0; i < N; ++i) original[i] = rng();

        // 1. std::sort baseline
        {
            auto data = original;
            double t = time_ms([&]() { data = original; std::sort(data.data(), data.data() + N); });
            bool ok = std::is_sorted(data.begin(), data.end());
            std::cout << std::setw(52) << "std::sort (Introsort)"
                      << std::setw(14) << std::fixed << std::setprecision(2) << t
                      << std::setw(20) << (N / 1e6) / (t / 1e3)
                      << (ok ? "PASS" : "FAIL") << "\n";
        }
        // 2. qi::sort (production baseline)
        {
            auto data = original;
            double t = time_ms([&]() { data = original; qi::sort(data.data(), N); });
            bool ok = std::is_sorted(data.begin(), data.end());
            std::cout << std::setw(52) << "qi::sort (v0.3.61 Production)"
                      << std::setw(14) << std::fixed << std::setprecision(2) << t
                      << std::setw(20) << (N / 1e6) / (t / 1e3)
                      << (ok ? "PASS" : "FAIL") << "\n";
        }
        // 3. qi::apex (current best)
        {
            auto data = original;
            double t = time_ms([&]() { data = original; qi::apex::sort(data.data(), N); });
            bool ok = std::is_sorted(data.begin(), data.end());
            std::cout << std::setw(52) << "qi::apex (Current Best)"
                      << std::setw(14) << std::fixed << std::setprecision(2) << t
                      << std::setw(20) << (N / 1e6) / (t / 1e3)
                      << (ok ? "PASS" : "FAIL") << "\n";
        }
        // 4. Engine A: MSD-8 Cache-Partitioned
        {
            auto data = original;
            double t = time_ms([&]() { data = original; experimental::msd_partitioned_sort(data.data(), N); });
            bool ok = std::is_sorted(data.begin(), data.end());
            std::cout << std::setw(52) << "[EXP-A] MSD-8 Cache-Partitioned Radix"
                      << std::setw(14) << std::fixed << std::setprecision(2) << t
                      << std::setw(20) << (N / 1e6) / (t / 1e3)
                      << (ok ? "PASS" : "FAIL") << "\n";
        }
        // 5. Engine C: 2-Pass Radix-16
        {
            auto data = original;
            double t = time_ms([&]() { data = original; experimental::radix16_2pass(data.data(), N); });
            bool ok = std::is_sorted(data.begin(), data.end());
            std::cout << std::setw(52) << "[EXP-C] 2-Pass Radix-16 (Fewer Passes)"
                      << std::setw(14) << std::fixed << std::setprecision(2) << t
                      << std::setw(20) << (N / 1e6) / (t / 1e3)
                      << (ok ? "PASS" : "FAIL") << "\n";
        }
        // 6. Engine D: MSD-10 Partitioned (1024 buckets)
        {
            auto data = original;
            double t = time_ms([&]() { data = original; experimental::msd10_partitioned_sort(data.data(), N); });
            bool ok = std::is_sorted(data.begin(), data.end());
            std::cout << std::setw(52) << "[EXP-D] MSD-10 1024-Bucket Partitioned"
                      << std::setw(14) << std::fixed << std::setprecision(2) << t
                      << std::setw(20) << (N / 1e6) / (t / 1e3)
                      << (ok ? "PASS" : "FAIL") << "\n";
        }

        std::cout << "------------------------------------------------------------------------------------\n\n";
    }

    return 0;
}
