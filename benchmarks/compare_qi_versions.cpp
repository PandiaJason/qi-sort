/*
===============================================================================
HONEST FULL VERIFICATION: REAL ORIGINAL v1 (Git Commit e85e530) vs CURRENT v2
===============================================================================
The Original v1 code is extracted VERBATIM from the first Git commit.
The Current v2 code is the live include/qi_radix.hpp header.

For each dataset we verify:
  1. Both produce 100% correctly sorted output (std::is_sorted)
  2. Both produce IDENTICAL sorted arrays (element-by-element match vs std::sort)
  3. Performance timing (7-trial median for stability)
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
#include <cassert>
#include <cstdint>
#include <numeric>
#include <array>
#include <memory>

// ═══════════════════════════════════════════════════════════════════════════════
// ORIGINAL QI::SORT v1.0 — VERBATIM FROM GIT COMMIT e85e530 (First Release)
// Placed under namespace qi_v1 to avoid symbol collision with current header
// ═══════════════════════════════════════════════════════════════════════════════
namespace qi_v1 {

using u32 = uint32_t;
using u64 = uint64_t;

enum class Radix : int { R8 = 8, R11 = 11, R16 = 16 };

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

struct SortOptions {
    size_t sampleSize = 8192;
    bool allowShortcuts = true;
    bool verbose = false;
};

namespace detail {

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
    u32 bitOr = 0, bitAnd = ~0u;
    size_t orderedCount = 0;
    size_t step = std::max<size_t>(1, n / sampleSize);
    u32 prevVal = 0;
    std::vector<u32> sampleBuf(sampleSize);

    for (size_t i = 0; i < sampleSize; ++i) {
        size_t idx = (i * step);
        if (idx >= n) idx = n - 1;
        u32 val = data[idx];
        sampleBuf[i] = val;
        bitOr |= val; bitAnd &= val;
        if (i > 0 && prevVal <= val) orderedCount++;
        prevVal = val;
        counts[0][val & 0xFFu]++;
        counts[1][(val >> 8) & 0xFFu]++;
        counts[2][(val >> 16) & 0xFFu]++;
        counts[3][(val >> 24) & 0xFFu]++;
    }

    state.bitOrSum = bitOr; state.bitAndSum = bitAnd;
    state.orderedness = (sampleSize > 1) ? static_cast<double>(orderedCount) / (sampleSize - 1) : 1.0;
    state.disorder = 1.0 - state.orderedness;

    std::sort(sampleBuf.begin(), sampleBuf.end());
    size_t uniqueCount = 0;
    for (size_t i = 0; i < sampleSize; ++i) {
        if (i == 0 || sampleBuf[i] != sampleBuf[i - 1]) uniqueCount++;
    }
    state.duplicateRatio = 1.0 - static_cast<double>(uniqueCount) / sampleSize;

    double entropySum = 0.0, concentrationSum = 0.0, effectiveSum = 0.0;
    for (int b = 0; b < 4; ++b) {
        ByteState& bs = state.bytes[b];
        bs.entropy = calculateEntropy(counts[b], sampleSize);
        double conc = 0.0; int occ = 0;
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
        entropySum += bs.entropy; concentrationSum += conc; effectiveSum += bs.effectiveStates;
    }

    state.averageEntropy = entropySum / 4.0;
    state.amplitudeConcentration = concentrationSum / 4.0;
    state.effectiveStates = effectiveSum / 4.0;
    state.lowByteComplexity = (state.bytes[0].entropy + state.bytes[1].entropy) / 2.0;
    state.highByteComplexity = (state.bytes[2].entropy + state.bytes[3].entropy) / 2.0;

    // ORIGINAL v1 cost model (verbatim from commit e85e530)
    u32 activeMask = state.bitOrSum ^ state.bitAndSum;
    double activeBits = 32.0;
    if (activeMask <= 0xFFFFu) activeBits = 16.0;
    else if (activeMask <= 0xFFFFFFu) activeBits = 24.0;

    double costR16 = n * (2.0 + (std::max(0.0, std::min(65536.0, state.bytes[0].effectiveStates * state.bytes[1].effectiveStates) - 32768.0) / 32768.0) * state.averageEntropy * (1.0 - state.amplitudeConcentration) * 2.8);
    double costR11 = n * (3.0 * (activeBits / 32.0) + state.averageEntropy * (1.0 - state.amplitudeConcentration) * 0.4);
    double costR8  = n * (4.0 * (activeBits / 32.0) + 0.05);

    if (state.orderedness > 0.98) { costR16 *= 0.05; costR11 *= 0.05; costR8 *= 0.05; }

    if (costR11 < costR16 && costR11 < costR8)       state.recommendedRadix = Radix::R11;
    else if (costR8 < costR16 && costR8 < costR11)   state.recommendedRadix = Radix::R8;
    else                                              state.recommendedRadix = Radix::R16;

    auto end = std::chrono::steady_clock::now();
    state.analysisTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    return state;
}

static inline void radixSort16(u32* data, size_t n, bool allowShortcuts) {
    if (n <= 1) return;
    if (allowShortcuts) {
        if (std::is_sorted(data, data + n)) return;
        bool isReverse = true;
        for (size_t i = 1; i < std::min<size_t>(n, 1024); ++i) {
            if (data[i - 1] < data[i]) { isReverse = false; break; }
        }
        if (isReverse && std::is_sorted(std::make_reverse_iterator(data + n), std::make_reverse_iterator(data))) {
            std::reverse(data, data + n); return;
        }
    }
    std::vector<u32> temp(n);
    u32* src = data; u32* dst = temp.data();
    auto count0_ptr = std::make_unique<std::array<size_t, 65536>>();
    auto count1_ptr = std::make_unique<std::array<size_t, 65536>>();
    auto& count0 = *count0_ptr; auto& count1 = *count1_ptr;
    count0.fill(0); count1.fill(0);
    for (size_t i = 0; i < n; ++i) { u32 val = src[i]; count0[val & 0xFFFFu]++; count1[val >> 16]++; }
    bool singlePass = (count1[0] == n);
    if (singlePass) {
        size_t sum = 0;
        for (int i = 0; i < 65536; ++i) { size_t c = count0[i]; count0[i] = sum; sum += c; }
        for (size_t i = 0; i < n; ++i) { u32 val = src[i]; dst[count0[val & 0xFFFFu]++] = val; }
        std::memcpy(data, dst, n * sizeof(u32)); return;
    }
    size_t sum0 = 0, sum1 = 0;
    for (int i = 0; i < 65536; ++i) {
        size_t c0 = count0[i]; count0[i] = sum0; sum0 += c0;
        size_t c1 = count1[i]; count1[i] = sum1; sum1 += c1;
    }
    size_t i = 0;
    for (; i + 3 < n; i += 4) {
        u32 v0=src[i],v1=src[i+1],v2=src[i+2],v3=src[i+3];
        __builtin_prefetch(&src[i+32],0,1);
        dst[count0[v0&0xFFFFu]++]=v0; dst[count0[v1&0xFFFFu]++]=v1;
        dst[count0[v2&0xFFFFu]++]=v2; dst[count0[v3&0xFFFFu]++]=v3;
    }
    for (; i < n; ++i) { u32 v=src[i]; dst[count0[v&0xFFFFu]++]=v; }
    i = 0;
    for (; i + 3 < n; i += 4) {
        u32 v0=dst[i],v1=dst[i+1],v2=dst[i+2],v3=dst[i+3];
        __builtin_prefetch(&dst[i+32],0,1);
        src[count1[v0>>16]++]=v0; src[count1[v1>>16]++]=v1;
        src[count1[v2>>16]++]=v2; src[count1[v3>>16]++]=v3;
    }
    for (; i < n; ++i) { u32 v=dst[i]; src[count1[v>>16]++]=v; }
}

static inline void radixSort11(u32* data, size_t n, bool allowShortcuts) {
    if (n <= 1) return;
    if (allowShortcuts && std::is_sorted(data, data + n)) return;
    std::vector<u32> temp(n);
    u32* src = data; u32* dst = temp.data();
    int shift = 0;
    while (shift < 32) {
        int currentBits = std::min(11, 32 - shift);
        uint32_t buckets = 1u << currentBits;
        std::vector<size_t> count(buckets, 0);
        uint32_t mask = buckets - 1;
        for (size_t i = 0; i < n; ++i) count[(src[i] >> shift) & mask]++;
        size_t sum = 0;
        for (uint32_t i = 0; i < buckets; ++i) { size_t c = count[i]; count[i] = sum; sum += c; }
        for (size_t i = 0; i < n; ++i) { uint32_t digit = (src[i] >> shift) & mask; dst[count[digit]++] = src[i]; }
        std::swap(src, dst);
        shift += currentBits;
    }
    if (src != data) std::memcpy(data, src, n * sizeof(u32));
}

static inline void radixSort8(u32* data, size_t n, bool allowShortcuts) {
    if (n <= 1) return;
    if (allowShortcuts && std::is_sorted(data, data + n)) return;
    std::vector<u32> temp(n);
    u32* src = data; u32* dst = temp.data();
    for (int shift = 0; shift < 32; shift += 8) {
        alignas(64) size_t count[256] = {0};
        for (size_t i = 0; i < n; ++i) count[(src[i] >> shift) & 0xFFu]++;
        size_t sum = 0;
        for (int i = 0; i < 256; ++i) { size_t c = count[i]; count[i] = sum; sum += c; }
        for (size_t i = 0; i < n; ++i) { u32 digit = (src[i] >> shift) & 0xFFu; dst[count[digit]++] = src[i]; }
        std::swap(src, dst);
    }
    if (src != data) std::memcpy(data, src, n * sizeof(u32));
}

} // namespace detail

// ORIGINAL v1 sort() dispatcher (verbatim logic)
inline void sort(u32* data, size_t n) {
    if (n <= 1) return;
    SortOptions options;
    State state = detail::analyzeData(data, n, options.sampleSize);
    switch (state.recommendedRadix) {
        case Radix::R8:  detail::radixSort8(data, n, options.allowShortcuts); break;
        case Radix::R11: detail::radixSort11(data, n, options.allowShortcuts); break;
        case Radix::R16: detail::radixSort16(data, n, options.allowShortcuts); break;
    }
}

} // namespace qi_v1

// ═══════════════════════════════════════════════════════════════════════════════
// CURRENT v2 — uses the live include/qi_radix.hpp header
// ═══════════════════════════════════════════════════════════════════════════════
#include "../include/qi_radix.hpp"

using Clock = std::chrono::high_resolution_clock;

// ═══════════════════════════════════════════════════════════════════════════════
// DATASET GENERATORS
// ═══════════════════════════════════════════════════════════════════════════════
static std::vector<uint32_t> generate(const std::string& name, size_t n) {
    std::vector<uint32_t> d(n);
    std::mt19937_64 rng(42);
    if (name == "Uniform Random") {
        std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
        for (size_t i = 0; i < n; ++i) d[i] = dist(rng);
    } else if (name == "Heavy Duplicates (0-255)") {
        std::uniform_int_distribution<uint32_t> dist(0, 255);
        for (size_t i = 0; i < n; ++i) d[i] = dist(rng);
    } else if (name == "Hash Join Keys") {
        for (size_t i = 0; i < n; ++i) { uint32_t h = uint32_t(i * 2654435761u); d[i] = h ^ (h >> 13); }
    } else if (name == "Nearly Sorted (95%)") {
        std::iota(d.begin(), d.end(), 0u);
        size_t swaps = n / 100;
        std::uniform_int_distribution<size_t> dist(0, n - 1);
        for (size_t i = 0; i < swaps; ++i) std::swap(d[dist(rng)], d[dist(rng)]);
    } else if (name == "All Zeros") {
        std::fill(d.begin(), d.end(), 0);
    } else if (name == "Reverse Sorted") {
        std::iota(d.begin(), d.end(), 0u);
        std::reverse(d.begin(), d.end());
    }
    return d;
}

int main() {
    const size_t N = 3000000;
    const int TRIALS = 7;

    std::vector<std::string> dists = {
        "Uniform Random", "Heavy Duplicates (0-255)", "Hash Join Keys",
        "Nearly Sorted (95%)", "All Zeros", "Reverse Sorted"
    };

    std::cout << "====================================================================================================\n";
    std::cout << "  HONEST FULL VERIFICATION: REAL ORIGINAL v1 (Git e85e530) vs CURRENT v2\n";
    std::cout << "  N = " << N << " Keys | " << TRIALS << "-Trial Median | Correctness + Performance\n";
    std::cout << "====================================================================================================\n\n";

    int total_tests = 0, passed_tests = 0, v1_wins = 0, v2_wins = 0, ties = 0;

    for (const auto& name : dists) {
        auto raw = generate(name, N);

        // ── CORRECTNESS ──
        auto c_v1 = raw, c_v2 = raw, c_ref = raw;
        qi_v1::sort(c_v1.data(), c_v1.size());
        qi::sort(c_v2.data(), c_v2.size());
        std::sort(c_ref.begin(), c_ref.end());

        bool v1_ok = std::is_sorted(c_v1.begin(), c_v1.end());
        bool v2_ok = std::is_sorted(c_v2.begin(), c_v2.end());
        bool v1_ref = (c_v1 == c_ref);
        bool v2_ref = (c_v2 == c_ref);
        bool match  = (c_v1 == c_v2);

        total_tests += 5;
        if (v1_ok) passed_tests++;
        if (v2_ok) passed_tests++;
        if (v1_ref) passed_tests++;
        if (v2_ref) passed_tests++;
        if (match) passed_tests++;

        // ── PERFORMANCE (7-trial median) ──
        std::vector<double> t1(TRIALS), t2(TRIALS);
        for (int t = 0; t < TRIALS; ++t) {
            { auto w = raw; auto s = Clock::now(); qi_v1::sort(w.data(), w.size()); t1[t] = std::chrono::duration<double,std::milli>(Clock::now()-s).count(); }
            { auto w = raw; auto s = Clock::now(); qi::sort(w.data(), w.size()); t2[t] = std::chrono::duration<double,std::milli>(Clock::now()-s).count(); }
        }
        std::sort(t1.begin(), t1.end());
        std::sort(t2.begin(), t2.end());
        double m1 = t1[TRIALS/2], m2 = t2[TRIALS/2];

        // ── Which radix each version picks ──
        auto st_v1 = qi_v1::detail::analyzeData(raw.data(), raw.size());
        auto st_v2 = qi::analyze(raw.data(), raw.size());
        auto radix_str = [](auto r) -> std::string {
            if ((int)r == 8) return "R-8";
            if ((int)r == 11) return "R-11";
            return "R-16";
        };

        std::cout << "----------------------------------------------------------------------------------------------------\n";
        std::cout << " DATASET: " << name << "\n";
        std::cout << "----------------------------------------------------------------------------------------------------\n";
        std::cout << "  Radix Selection:  v1 picks " << radix_str(st_v1.recommendedRadix)
                  << "  |  v2 picks " << radix_str(st_v2.recommendedRadix);
        if ((int)st_v1.recommendedRadix == (int)st_v2.recommendedRadix) std::cout << "  (SAME)\n";
        else std::cout << "  (DIFFERENT)\n";

        std::cout << "\n  CORRECTNESS:\n";
        std::cout << "    v1 sorted correctly:     " << (v1_ok ? "✓ PASS" : "✗ FAIL") << "\n";
        std::cout << "    v2 sorted correctly:     " << (v2_ok ? "✓ PASS" : "✗ FAIL") << "\n";
        std::cout << "    v1 matches std::sort:    " << (v1_ref ? "✓ PASS" : "✗ FAIL") << "\n";
        std::cout << "    v2 matches std::sort:    " << (v2_ref ? "✓ PASS" : "✗ FAIL") << "\n";
        std::cout << "    v1 == v2 (identical):    " << (match ? "✓ PASS" : "✗ FAIL") << "\n";

        std::cout << "\n  PERFORMANCE (7-Trial Median):\n";
        std::cout << "    Original v1 (e85e530):  " << std::fixed << std::setprecision(2) << m1 << " ms  ("
                  << (int)(N / m1 / 1000.0) << " MKeys/s)\n";
        std::cout << "    Current  v2 (latest) :  " << std::fixed << std::setprecision(2) << m2 << " ms  ("
                  << (int)(N / m2 / 1000.0) << " MKeys/s)\n";

        double diff = ((m1 - m2) / m1) * 100.0;
        if (std::abs(diff) < 1.0) {
            std::cout << "    --> TIE: Both versions within 1% (" << std::setprecision(2) << std::abs(diff) << "% difference)\n\n";
            ties++;
        } else if (m2 < m1) {
            std::cout << "    --> WINNER: v2 (Current) is " << std::setprecision(2) << std::abs(diff)
                      << "% FASTER (" << std::setprecision(3) << (m1/m2) << "x speedup)\n\n";
            v2_wins++;
        } else {
            std::cout << "    --> WINNER: v1 (Original) is " << std::setprecision(2) << std::abs(diff)
                      << "% FASTER (" << std::setprecision(3) << (m2/m1) << "x speedup)\n\n";
            v1_wins++;
        }
    }

    std::cout << "====================================================================================================\n";
    std::cout << "  FINAL HONEST SCORECARD\n";
    std::cout << "====================================================================================================\n";
    std::cout << "  Correctness:       " << passed_tests << " / " << total_tests << " PASSED";
    if (passed_tests == total_tests) std::cout << " (100% PERFECT)";
    std::cout << "\n";
    std::cout << "  v1 (Original) wins: " << v1_wins << " / " << dists.size() << " datasets\n";
    std::cout << "  v2 (Current)  wins: " << v2_wins << " / " << dists.size() << " datasets\n";
    std::cout << "  Ties (<1% diff):    " << ties   << " / " << dists.size() << " datasets\n";

    if (v2_wins > v1_wins)
        std::cout << "\n  HONEST VERDICT: CURRENT v2 IS THE FASTER ENGINE\n";
    else if (v1_wins > v2_wins)
        std::cout << "\n  HONEST VERDICT: ORIGINAL v1 IS THE FASTER ENGINE\n";
    else
        std::cout << "\n  HONEST VERDICT: BOTH ENGINES ARE EQUALLY FAST (core radix kernels unchanged)\n";

    std::cout << "====================================================================================================\n";
    return 0;
}
