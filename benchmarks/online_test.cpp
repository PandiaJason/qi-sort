/*
===============================================================================
QI-SORT MULTI-TRIAL 7-WAY BENCHMARK (5-RUN STATISTICAL AVERAGING)
===============================================================================
Runs 5 trials per algorithm to measure Average, Min, and Max execution times:
  1. std::sort          — Introsort (QuickSort + HeapSort)
  2. std::stable_sort   — Timsort / MergeSort
  3. Classic QuickSort  — Hoare-partitioned recursive QuickSort
  4. Plain Radix-8      — Fixed 4-Pass LSD Radix Sort (256 Buckets, 2 KB Cache)
  5. Plain Radix-11     — Fixed 3-Pass LSD Radix Sort (2048 Buckets, 16 KB Cache)
  6. Plain Radix-16     — Fixed 2-Pass LSD Radix Sort (65536 Buckets, 512 KB Cache)
  7. qi::sort           — Quantum-Inspired Adaptive Radix Engine
===============================================================================
*/

#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>
#include <iomanip>
#include <cstring>
#include <cmath>
#include <array>
#include <cstdint>
#include <memory>
#include <functional>
#include <string>
#include <numeric>

// ─── CLASSIC HOARE QUICKSORT IMPLEMENTATION ─────────────────────────────────
static void classic_quicksort(uint32_t* arr, int low, int high) {
    if (low < high) {
        int mid = low + (high - low) / 2;
        if (arr[mid] < arr[low]) std::swap(arr[low], arr[mid]);
        if (arr[high] < arr[low]) std::swap(arr[low], arr[high]);
        if (arr[high] < arr[mid]) std::swap(arr[mid], arr[high]);

        uint32_t pivot = arr[mid];
        int i = low - 1;
        int j = high + 1;

        while (true) {
            do { i++; } while (arr[i] < pivot);
            do { j--; } while (arr[j] > pivot);
            if (i >= j) break;
            std::swap(arr[i], arr[j]);
        }

        classic_quicksort(arr, low, j);
        classic_quicksort(arr, j + 1, high);
    }
}

// ─── QI-SORT & PLAIN RADIX KERNELS DEFINITIONS ──────────────────────────────
namespace qi {

using u32 = uint32_t;
using u64 = uint64_t;

enum class Radix : int { R8 = 8, R11 = 11, R16 = 16 };

struct SortOptions {
    size_t sampleSize = 8192;
    bool allowShortcuts = true;
    bool verbose = false;
};

struct ByteState {
    double entropy = 0.0;
    double amplitudeConcentration = 0.0;
    double effectiveStates = 0.0;
    int occupied = 0;
    std::array<double, 256> probability{};
    std::array<double, 256> amplitude{};
};

struct State {
    double averageEntropy = 0.0;
    double amplitudeConcentration = 0.0;
    double effectiveStates = 0.0;
    double duplicateRatio = 0.0;
    double orderedness = 0.0;
    u32 bitOrSum = 0;
    u32 bitAndSum = ~0u;
    std::array<ByteState, 4> bytes{};
    size_t sampleSize = 0;
    double analysisTimeMs = 0.0;
    Radix recommendedRadix = Radix::R16;
};

namespace detail {

static inline State analyzeData(const u32* data, size_t n, size_t targetSampleSize = 8192) {
    auto start = std::chrono::steady_clock::now();
    State state;
    size_t sampleSize = std::min<size_t>(targetSampleSize, n);
    state.sampleSize = sampleSize;

    std::array<std::array<u64, 256>, 4> counts{};
    u32 bitOr = 0;
    u32 bitAnd = ~0u;
    size_t orderedCount = 0;

    for (size_t i = 0; i < sampleSize; ++i) {
        u32 val = data[i];
        bitOr |= val;
        bitAnd &= val;

        counts[0][val & 0xFFu]++;
        counts[1][(val >> 8) & 0xFFu]++;
        counts[2][(val >> 16) & 0xFFu]++;
        counts[3][(val >> 24) & 0xFFu]++;

        if (i > 0 && data[i - 1] <= data[i]) orderedCount++;
    }

    state.bitOrSum = bitOr;
    state.bitAndSum = bitAnd;
    state.orderedness = (sampleSize > 1) ? (static_cast<double>(orderedCount) / (sampleSize - 1)) : 1.0;

    double maxDuplicateInSample = 0;
    for (int b = 0; b < 4; ++b) {
        for (int i = 0; i < 256; ++i) {
            if (counts[b][i] > maxDuplicateInSample) maxDuplicateInSample = counts[b][i];
        }
    }
    state.duplicateRatio = maxDuplicateInSample / static_cast<double>(sampleSize);

    double entropySum = 0.0, concentrationSum = 0.0, effectiveSum = 0.0;

    for (int b = 0; b < 4; ++b) {
        auto& bs = state.bytes[b];
        double h = 0.0, conc = 0.0;
        int occ = 0;

        for (int i = 0; i < 256; ++i) {
            u64 c = counts[b][i];
            if (c == 0) continue;
            occ++;
            double p = static_cast<double>(c) / static_cast<double>(sampleSize);
            h -= p * std::log2(p);
            bs.probability[i] = p;
            bs.amplitude[i] = std::sqrt(p);
            conc += p * p;
        }

        bs.entropy = h / 8.0;
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

    double orderFactor = std::max(0.0, (state.orderedness - 0.50) * 2.0);
    double r16BucketDispersion = (state.bytes[0].effectiveStates * state.bytes[1].effectiveStates) / 65536.0;
    double r16CachePenalty = (r16BucketDispersion > 0.20) ? (r16BucketDispersion * state.averageEntropy * (1.0 - orderFactor) * 1.15) : 0.0;
    double r16Passes = (state.bitOrSum <= 0xFFFFu) ? 1.0 : 2.0;
    double costR16 = n * (r16Passes + r16CachePenalty);
    double costR11 = n * 3.0;
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

static inline void radixSort16(u32* data, size_t n, bool allowShortcuts) {
    if (n <= 1) return;
    if (allowShortcuts && std::is_sorted(data, data + n)) return;

    std::vector<u32> temp(n);
    u32* src = data;
    u32* dst = temp.data();

    std::unique_ptr<std::array<size_t, 65536>> count0_ptr(new std::array<size_t, 65536>());
    std::unique_ptr<std::array<size_t, 65536>> count1_ptr(new std::array<size_t, 65536>());
    auto& count0 = *count0_ptr;
    auto& count1 = *count1_ptr;

    count0.fill(0); count1.fill(0);

    for (size_t i = 0; i < n; ++i) {
        u32 val = src[i];
        count0[val & 0xFFFFu]++;
        count1[val >> 16]++;
    }

    if (count1[0] == n) {
        size_t sum = 0;
        for (int i = 0; i < 65536; ++i) { size_t c = count0[i]; count0[i] = sum; sum += c; }
        for (size_t i = 0; i < n; ++i) { u32 val = src[i]; dst[count0[val & 0xFFFFu]++] = val; }
        std::memcpy(data, dst, n * sizeof(u32));
        return;
    }

    size_t sum0 = 0, sum1 = 0;
    for (int i = 0; i < 65536; ++i) {
        size_t c0 = count0[i]; count0[i] = sum0; sum0 += c0;
        size_t c1 = count1[i]; count1[i] = sum1; sum1 += c1;
    }

    for (size_t i = 0; i < n; ++i) { u32 v = src[i]; dst[count0[v & 0xFFFFu]++] = v; }
    for (size_t i = 0; i < n; ++i) { u32 v = dst[i]; src[count1[v >> 16]++] = v; }
}

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
        for (uint32_t i = 0; i < buckets; ++i) { size_t c = count[i]; count[i] = sum; sum += c; }
        for (size_t i = 0; i < n; ++i) { dst[count[(src[i] >> shift) & mask]++] = src[i]; }
        std::swap(src, dst);
        shift += currentBits;
    }
    if (src != data) std::memcpy(data, src, n * sizeof(u32));
}

static inline void radixSort8(u32* data, size_t n, bool allowShortcuts) {
    if (n <= 1) return;
    if (allowShortcuts && std::is_sorted(data, data + n)) return;

    std::vector<u32> temp(n);
    u32* src = data;
    u32* dst = temp.data();

    for (int shift = 0; shift < 32; shift += 8) {
        size_t count[256] = {0};
        for (size_t i = 0; i < n; ++i) count[(src[i] >> shift) & 0xFFu]++;
        size_t sum = 0;
        for (int i = 0; i < 256; ++i) { size_t c = count[i]; count[i] = sum; sum += c; }
        for (size_t i = 0; i < n; ++i) { dst[count[(src[i] >> shift) & 0xFFu]++] = src[i]; }
        std::swap(src, dst);
    }
    if (src != data) std::memcpy(data, src, n * sizeof(u32));
}

} // namespace detail

// Fixed Plain Radix Implementations (No Sensing)
inline void plain_radix8(u32* data, size_t n)  { detail::radixSort8(data, n, false); }
inline void plain_radix11(u32* data, size_t n) { detail::radixSort11(data, n, false); }
inline void plain_radix16(u32* data, size_t n) { detail::radixSort16(data, n, false); }

// Adaptive qi::sort Engine
inline void sort(u32* data, size_t n, SortOptions options = SortOptions{}) {
    if (n <= 1) return;
    State state = detail::analyzeData(data, n, options.sampleSize);

    if (options.verbose) {
        std::cout << "[QI-Sensing] N=" << n
                  << " | Selected=" << (state.recommendedRadix == Radix::R16 ? "RADIX-16" : (state.recommendedRadix == Radix::R11 ? "RADIX-11" : "RADIX-8"))
                  << " | Entropy=" << std::fixed << std::setprecision(4) << state.averageEntropy
                  << " | N_eff=" << state.effectiveStates
                  << " | SensingTime=" << std::setprecision(2) << state.analysisTimeMs << "ms\n";
    }

    switch (state.recommendedRadix) {
        case Radix::R8:  detail::radixSort8(data, n, options.allowShortcuts); break;
        case Radix::R11: detail::radixSort11(data, n, options.allowShortcuts); break;
        case Radix::R16: detail::radixSort16(data, n, options.allowShortcuts); break;
    }
}

inline void sort(std::vector<u32>& data, SortOptions options = SortOptions{}) {
    sort(data.data(), data.size(), options);
}

} // namespace qi

// ─── STANDALONE HELPER FUNCTIONS FOR BENCHMARK WRAPPERS ────────────────────
static void run_std_sort(std::vector<uint32_t>& d) {
    std::sort(d.begin(), d.end());
}
static void run_timsort(std::vector<uint32_t>& d) {
    std::stable_sort(d.begin(), d.end());
}
static void run_quicksort(std::vector<uint32_t>& d) {
    classic_quicksort(d.data(), 0, (int)d.size() - 1);
}
static void run_pradix8(std::vector<uint32_t>& d) {
    qi::plain_radix8(d.data(), d.size());
}
static void run_pradix11(std::vector<uint32_t>& d) {
    qi::plain_radix11(d.data(), d.size());
}
static void run_pradix16(std::vector<uint32_t>& d) {
    qi::plain_radix16(d.data(), d.size());
}
static void run_qisort(std::vector<uint32_t>& d) {
    qi::SortOptions opts;
    opts.verbose = false;
    qi::sort(d, opts);
}

// ===============================================================================
// BENCHMARK MAIN FUNCTION (MULTI-TRIAL 5-RUN STATISTICAL BENCHMARK)
// ===============================================================================

using Clock = std::chrono::high_resolution_clock;

struct BenchmarkResult {
    std::string name;
    double avgMs = 0.0;
    double minMs = 0.0;
    double maxMs = 0.0;
    bool allPassed = true;
};

static BenchmarkResult run_multi_trial(const std::string& name, size_t N, int numTrials,
                                       const std::vector<uint32_t>& referenceData,
                                       void (*sortFn)(std::vector<uint32_t>&)) {
    BenchmarkResult res;
    res.name = name;
    std::vector<double> times;

    for (int t = 0; t < numTrials; ++t) {
        auto d = referenceData; // fresh copy per trial
        auto start = Clock::now();
        sortFn(d);
        auto end = Clock::now();

        double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
        times.push_back(elapsed);

        // Verification against std::sort
        static std::vector<uint32_t> expected;
        if (t == 0) {
            expected = referenceData;
            std::sort(expected.begin(), expected.end());
        }
        if (d != expected) {
            res.allPassed = false;
        }
    }

    res.minMs = *std::min_element(times.begin(), times.end());
    res.maxMs = *std::max_element(times.begin(), times.end());
    double sum = std::accumulate(times.begin(), times.end(), 0.0);
    res.avgMs = sum / times.size();

    return res;
}

int main() {
    const size_t N = 3000000;  // 3 Million keys
    const int TRIALS = 5;       // 5 iterations for statistical accuracy

    std::cout << "=================================================================\n";
    std::cout << "  QI-SORT MULTI-TRIAL BENCHMARK (STATISTICAL AVERAGING OVER " << TRIALS << " RUNS)\n";
    std::cout << "  N = " << N << " 32-bit unsigned integers\n";
    std::cout << "=================================================================\n\n";

    // Run one initial sensing display call to show parameters
    std::mt19937_64 rng(42);
    std::vector<uint32_t> initData(N);
    std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
    for (auto& x : initData) x = dist(rng);
    
    qi::SortOptions opts;
    opts.verbose = true;
    auto sensingCopy = initData;
    qi::sort(sensingCopy, opts);
    std::cout << "\nRunning " << TRIALS << " trials per algorithm...\n\n";

    // Run statistical trials using clean static helper function pointers
    BenchmarkResult r_std     = run_multi_trial("std::sort (Introsort)",         N, TRIALS, initData, run_std_sort);
    BenchmarkResult r_tim     = run_multi_trial("std::stable_sort (Timsort)",    N, TRIALS, initData, run_timsort);
    BenchmarkResult r_qsort   = run_multi_trial("Classic QuickSort (Hoare)",     N, TRIALS, initData, run_quicksort);
    BenchmarkResult r_pradix8 = run_multi_trial("Plain Radix-8 (Fixed 4-Pass)", N, TRIALS, initData, run_pradix8);
    BenchmarkResult r_pradix11= run_multi_trial("Plain Radix-11 (Fixed 3-Pass)",N, TRIALS, initData, run_pradix11);
    BenchmarkResult r_pradix16= run_multi_trial("Plain Radix-16 (Fixed 2-Pass)",N, TRIALS, initData, run_pradix16);
    BenchmarkResult r_qi      = run_multi_trial("qi::sort (Adaptive Engine)",    N, TRIALS, initData, run_qisort);

    std::vector<BenchmarkResult> results;
    results.push_back(r_std);
    results.push_back(r_tim);
    results.push_back(r_qsort);
    results.push_back(r_pradix8);
    results.push_back(r_pradix11);
    results.push_back(r_pradix16);
    results.push_back(r_qi);

    std::cout << "----------------------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(32) << "Algorithm"
              << std::setw(14) << "Avg (ms)"
              << std::setw(12) << "Min (ms)"
              << std::setw(12) << "Max (ms)"
              << std::setw(14) << "Avg Speed"
              << "Status\n";
    std::cout << "----------------------------------------------------------------------------------------\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        double mk = N / r.avgMs / 1000.0;
        double vs = r_std.avgMs / r.avgMs;
        std::cout << std::left << std::setw(32) << r.name
                  << std::setw(14) << std::fixed << std::setprecision(2) << r.avgMs
                  << std::setw(12) << std::fixed << std::setprecision(2) << r.minMs
                  << std::setw(12) << std::fixed << std::setprecision(2) << r.maxMs
                  << std::setw(14) << (std::to_string(vs).substr(0, 4) + "x")
                  << (r.allPassed ? "PASS" : "FAIL") << "\n";
    }

    std::cout << "----------------------------------------------------------------------------------------\n";
    std::cout << "qi::sort Average Speedup vs std::sort      : " << std::setprecision(2) << (r_std.avgMs / r_qi.avgMs) << "x FASTER\n";
    std::cout << "qi::sort Average Speedup vs Timsort       : " << std::setprecision(2) << (r_tim.avgMs / r_qi.avgMs) << "x FASTER\n";
    std::cout << "qi::sort Average Speedup vs QuickSort     : " << std::setprecision(2) << (r_qsort.avgMs / r_qi.avgMs) << "x FASTER\n";
    std::cout << "qi::sort Average Speedup vs Plain Radix-8 : " << std::setprecision(2) << (r_pradix8.avgMs / r_qi.avgMs) << "x FASTER\n";
    std::cout << "qi::sort Average Speedup vs Plain Radix-11: " << std::setprecision(2) << (r_pradix11.avgMs / r_qi.avgMs) << "x FASTER\n";
    std::cout << "qi::sort Average Speedup vs Plain Radix-16: " << std::setprecision(2) << (r_pradix16.avgMs / r_qi.avgMs) << "x FASTER\n";
    std::cout << "========================================================================================\n\n";

    return 0;
}
