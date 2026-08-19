/*
===============================================================================
QI-RADIX: PROBABILITY-AMPLITUDE-INSPIRED DISTRIBUTION ANALYSIS
FOR ADAPTIVE RADIX SORTING — COMPREHENSIVE EXPERIMENTAL SUITE
===============================================================================

Classical 32-bit integer data
Classical CPU execution
No quantum hardware / No quantum simulation

Key Features:
    - Quantum-Inspired State Vector: \psi_i = \sqrt{p_i}, IPR = \sum |\psi_i|^4, N_{eff} = 1 / IPR
    - Real QI-Driven Adaptive Cost Model: Uses IPR, effective states, byte entropy, duplicate ratio,
      and active bit masks to estimate cache thrashing & bucket collision penalties.
    - True Radix Selection: Dynamically selects R8, R11, or R16 based on state vector observables.
    - Extended Baselines: std::sort, std::stable_sort, Fixed Radix-8, Fixed Radix-11, Fixed Radix-16,
      Classical Heuristic Adaptive, and Full QI-Radix.
    - Full Metric Reporting: Both Mean Per-Dataset Speedup AND Aggregate Benchmark Speedup.
    - Ablation Experiments: Isolates early-exit O(N) shortcuts from pure radix sorting engine.

===============================================================================
Build:

    g++ -O3 -std=c++17 -march=native qi_radix_final.cpp -o qi_radix

Run:

    ./qi_radix

===============================================================================
*/

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <vector>

using namespace std;
using Clock = chrono::steady_clock;

using u32 = uint32_t;
using u64 = uint64_t;

// ============================================================================
// CONFIGURATION
// ============================================================================

static constexpr size_t N = 1'000'000;
static constexpr int STATE_SAMPLE_SIZE = 8192;
static constexpr int TIMING_TRIALS = 7;

// ============================================================================
// RADIX ENUM
// ============================================================================

enum class Radix {
    R8 = 8,
    R11 = 11,
    R16 = 16
};

static const char* radixName(Radix r) {
    switch (r) {
        case Radix::R8:  return "RADIX-8";
        case Radix::R11: return "RADIX-11";
        case Radix::R16: return "RADIX-16";
    }
    return "UNKNOWN";
}

// ============================================================================
// TIMING UTILITY
// ============================================================================

struct Timing {
    double median = 0.0;
    double mean = 0.0;
    double stddev = 0.0;
};

static Timing summarize(vector<double> values) {
    sort(values.begin(), values.end());
    double sum = accumulate(values.begin(), values.end(), 0.0);
    double mean = sum / static_cast<double>(values.size());
    double variance = 0.0;

    for (double x : values) {
        double d = x - mean;
        variance += d * d;
    }
    variance /= static_cast<double>(values.size());

    double median;
    if (values.size() % 2 == 0) {
        size_t m = values.size() / 2;
        median = (values[m - 1] + values[m]) / 2.0;
    } else {
        median = values[values.size() / 2];
    }

    return { median, mean, sqrt(variance) };
}

// ============================================================================
// BYTE STATE & QI STATE DEFINITIONS
// ============================================================================

struct ByteState {
    double entropy = 0.0;
    double amplitudeConcentration = 0.0; // IPR = sum(p_i^2) = sum(|psi_i|^4)
    double effectiveStates = 0.0;         // N_eff = 1 / IPR
    int occupied = 0;
    array<double, 256> probability{};
    array<double, 256> amplitude{};
};

struct QIState {
    double averageEntropy = 0.0;
    double amplitudeConcentration = 0.0; // Average IPR across bytes
    double effectiveStates = 0.0;         // Average N_eff across bytes
    double duplicateRatio = 0.0;
    double orderedness = 0.0;
    double disorder = 0.0;
    double lowByteComplexity = 0.0;
    double highByteComplexity = 0.0;
    double stateScore = 0.0;
    u32 bitOrSum = 0;
    u32 bitAndSum = ~0u;
    array<ByteState, 4> bytes{};
    size_t sampleSize = 0;
    double analysisTime = 0.0;
};

// ============================================================================
// DATASET TYPES & GENERATOR
// ============================================================================

enum class DatasetType {
    RANDOM,
    DUPLICATE_HEAVY,
    CLUSTERED,
    SORTED,
    REVERSE,
    LOW_RANGE,
    ALTERNATING,
    ALMOST_SORTED,
    POWER_DISTRIBUTION,
    SAWTOOTH
};

static const char* datasetName(DatasetType type) {
    switch (type) {
        case DatasetType::RANDOM:             return "RANDOM";
        case DatasetType::DUPLICATE_HEAVY:    return "DUPLICATE-HEAVY";
        case DatasetType::CLUSTERED:          return "CLUSTERED";
        case DatasetType::SORTED:             return "SORTED";
        case DatasetType::REVERSE:            return "REVERSE";
        case DatasetType::LOW_RANGE:          return "LOW-RANGE";
        case DatasetType::ALTERNATING:        return "ALTERNATING";
        case DatasetType::ALMOST_SORTED:      return "ALMOST-SORTED";
        case DatasetType::POWER_DISTRIBUTION: return "POWER-DISTRIBUTION";
        case DatasetType::SAWTOOTH:           return "SAWTOOTH";
    }
    return "UNKNOWN";
}

static vector<u32> generateDataset(DatasetType type, size_t n, uint64_t seed) {
    vector<u32> data(n);
    mt19937_64 rng(seed);

    switch (type) {
        case DatasetType::RANDOM: {
            uniform_int_distribution<u32> dist(0, numeric_limits<u32>::max());
            for (auto& x : data) x = dist(rng);
            break;
        }
        case DatasetType::DUPLICATE_HEAVY: {
            uniform_int_distribution<u32> values(0, 15);
            for (auto& x : data) x = values(rng);
            break;
        }
        case DatasetType::CLUSTERED: {
            uniform_int_distribution<u32> low(0, 255);
            uniform_int_distribution<u32> cluster(0, 3);
            for (auto& x : data) {
                u32 c = cluster(rng);
                x = (c << 24) | (0x00100000u) | low(rng);
            }
            break;
        }
        case DatasetType::SORTED: {
            for (size_t i = 0; i < n; ++i) data[i] = static_cast<u32>(i);
            break;
        }
        case DatasetType::REVERSE: {
            for (size_t i = 0; i < n; ++i) data[i] = static_cast<u32>(n - i);
            break;
        }
        case DatasetType::LOW_RANGE: {
            uniform_int_distribution<u32> dist(0, 65535);
            for (auto& x : data) x = dist(rng);
            break;
        }
        case DatasetType::ALTERNATING: {
            for (size_t i = 0; i < n; ++i) data[i] = (i & 1) ? 0xAAAAAAAAu : 0x55555555u;
            break;
        }
        case DatasetType::ALMOST_SORTED: {
            for (size_t i = 0; i < n; ++i) data[i] = static_cast<u32>(i);
            uniform_int_distribution<size_t> pos(0, n - 1);
            size_t swaps = n / 1000;
            for (size_t i = 0; i < swaps; ++i) {
                size_t a = pos(rng);
                size_t b = pos(rng);
                swap(data[a], data[b]);
            }
            break;
        }
        case DatasetType::POWER_DISTRIBUTION: {
            uniform_real_distribution<double> dist(0.0, 1.0);
            for (auto& x : data) {
                double u = dist(rng);
                double value = pow(u, 12.0) * static_cast<double>(numeric_limits<u32>::max());
                x = static_cast<u32>(value);
            }
            break;
        }
        case DatasetType::SAWTOOTH: {
            for (size_t i = 0; i < n; ++i) {
                u32 x = static_cast<u32>(i & 0xFFFF);
                x |= static_cast<u32>((i / 65536) & 0xFF) << 16;
                x |= static_cast<u32>((i / 1024) & 0xFF) << 24;
                data[i] = x;
            }
            break;
        }
    }
    return data;
}

// ============================================================================
// HELPER FUNCTIONS & QI STATE CONSTRUCTION
// ============================================================================

static inline uint8_t getByte(u32 value, int byte) {
    return static_cast<uint8_t>((value >> (byte * 8)) & 0xFFu);
}

static double entropy(const array<u64, 256>& counts, size_t n) {
    if (n == 0) return 0.0;
    double h = 0.0;
    for (u64 c : counts) {
        if (c == 0) continue;
        double p = static_cast<double>(c) / static_cast<double>(n);
        h -= p * log2(p);
    }
    return h / 8.0; // Normalized byte entropy (0.0 to 1.0)
}

static QIState constructQIState(const vector<u32>& data) {
    auto start = Clock::now();
    QIState state;
    size_t n = data.size();
    size_t sampleSize = min<size_t>(STATE_SAMPLE_SIZE, n);
    state.sampleSize = sampleSize;

    array<array<u64, 256>, 4> counts{};
    u32 bitOr = 0;
    u32 bitAnd = ~0u;
    size_t orderedCount = 0;

    size_t step = max<size_t>(1, n / sampleSize);
    u32 prevVal = 0;

    alignas(64) u32 sampleBuf[STATE_SAMPLE_SIZE];

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
    state.orderedness = (sampleSize > 1) ? (double)orderedCount / (sampleSize - 1) : 1.0;
    state.disorder = 1.0 - state.orderedness;

    // Fast duplicate estimation on sampled array
    sort(sampleBuf, sampleBuf + sampleSize);
    size_t uniqueCount = 0;
    for (size_t i = 0; i < sampleSize; ++i) {
        if (i == 0 || sampleBuf[i] != sampleBuf[i - 1]) uniqueCount++;
    }
    state.duplicateRatio = 1.0 - (double)uniqueCount / sampleSize;

    double entropySum = 0.0;
    double concentrationSum = 0.0;
    double effectiveSum = 0.0;

    for (int b = 0; b < 4; ++b) {
        ByteState& bs = state.bytes[b];
        bs.entropy = entropy(counts[b], sampleSize);

        double conc = 0.0;
        int occ = 0;
        for (int i = 0; i < 256; ++i) {
            if (counts[b][i] == 0) continue;
            occ++;
            double p = (double)counts[b][i] / sampleSize;
            bs.probability[i] = p;
            bs.amplitude[i] = sqrt(p); // Quantum-inspired probability amplitude
            conc += p * p;              // IPR = sum(|psi_i|^4) = sum(p_i^2)
        }
        bs.amplitudeConcentration = conc;
        bs.effectiveStates = (conc > 0.0) ? (1.0 / conc) : 0.0; // N_eff = 1 / IPR
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

    auto end = Clock::now();
    state.analysisTime = chrono::duration<double, milli>(end - start).count();
    return state;
}

// ============================================================================
// INDEPENDENT RADIX SORT IMPLEMENTATIONS
// ============================================================================

// Radix-16 Implementation with Heap-allocated 64K histograms & Dual-histogram scan
static void radixSort16(vector<u32>& data, bool allowShortcuts = true) {
    size_t n = data.size();
    if (n <= 1) return;

    if (allowShortcuts) {
        if (std::is_sorted(data.begin(), data.end())) return;

        bool isReverse = true;
        for (size_t i = 1; i < min<size_t>(n, 1024); ++i) {
            if (data[i - 1] < data[i]) { isReverse = false; break; }
        }
        if (isReverse && std::is_sorted(data.rbegin(), data.rend())) {
            std::reverse(data.begin(), data.end());
            return;
        }
    }

    vector<u32> temp(n);
    u32* src = data.data();
    u32* dst = temp.data();

    auto count0_ptr = std::make_unique<array<size_t, 65536>>();
    auto count1_ptr = std::make_unique<array<size_t, 65536>>();
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
        memcpy(data.data(), dst, n * sizeof(u32));
        return;
    }

    size_t sum0 = 0, sum1 = 0;
    for (int i = 0; i < 65536; ++i) {
        size_t c0 = count0[i];
        count0[i] = sum0;
        sum0 += c0;

        size_t c1 = count1[i];
        count1[i] = sum1;
        sum1 += c1;
    }

    // Pass 1 (Lower 16 bits)
    size_t i = 0;
    for (; i + 3 < n; i += 4) {
        u32 v0 = src[i], v1 = src[i+1], v2 = src[i+2], v3 = src[i+3];
        __builtin_prefetch(&src[i + 32], 0, 1);
        dst[count0[v0 & 0xFFFFu]++] = v0;
        dst[count0[v1 & 0xFFFFu]++] = v1;
        dst[count0[v2 & 0xFFFFu]++] = v2;
        dst[count0[v3 & 0xFFFFu]++] = v3;
    }
    for (; i < n; ++i) {
        u32 v = src[i];
        dst[count0[v & 0xFFFFu]++] = v;
    }

    // Pass 2 (Upper 16 bits)
    i = 0;
    for (; i + 3 < n; i += 4) {
        u32 v0 = dst[i], v1 = dst[i+1], v2 = dst[i+2], v3 = dst[i+3];
        __builtin_prefetch(&dst[i + 32], 0, 1);
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

// Radix-11 Implementation (3 passes: 11 bits, 11 bits, 10 bits)
static void radixSort11(vector<u32>& data, bool allowShortcuts = true) {
    size_t n = data.size();
    if (n <= 1) return;
    if (allowShortcuts && std::is_sorted(data.begin(), data.end())) return;

    vector<u32> temp(n);
    u32* src = data.data();
    u32* dst = temp.data();

    int shift = 0;
    while (shift < 32) {
        int currentBits = min(11, 32 - shift);
        uint32_t buckets = 1u << currentBits;
        vector<size_t> count(buckets, 0);
        uint32_t mask = buckets - 1;

        for (size_t i = 0; i < n; ++i) {
            count[(src[i] >> shift) & mask]++;
        }

        size_t sum = 0;
        for (uint32_t i = 0; i < buckets; ++i) {
            size_t c = count[i];
            count[i] = sum;
            sum += c;
        }

        for (size_t i = 0; i < n; ++i) {
            uint32_t digit = (src[i] >> shift) & mask;
            dst[count[digit]++] = src[i];
        }
        swap(src, dst);
        shift += currentBits;
    }

    if (src != data.data()) memcpy(data.data(), src, n * sizeof(u32));
}

// Radix-8 Implementation (4 passes: 8 bits each)
static void radixSort8(vector<u32>& data, bool allowShortcuts = true) {
    size_t n = data.size();
    if (n <= 1) return;
    if (allowShortcuts && std::is_sorted(data.begin(), data.end())) return;

    vector<u32> temp(n);
    u32* src = data.data();
    u32* dst = temp.data();

    for (int shift = 0; shift < 32; shift += 8) {
        alignas(64) size_t count[256] = {0};
        for (size_t i = 0; i < n; ++i) count[(src[i] >> shift) & 0xFFu]++;

        size_t sum = 0;
        for (int i = 0; i < 256; ++i) {
            size_t c = count[i];
            count[i] = sum;
            sum += c;
        }

        for (size_t i = 0; i < n; ++i) {
            u32 digit = (src[i] >> shift) & 0xFFu;
            dst[count[digit]++] = src[i];
        }
        swap(src, dst);
    }
    if (src != data.data()) memcpy(data.data(), src, n * sizeof(u32));
}

static void radixSort(vector<u32>& data, Radix r, bool allowShortcuts = true) {
    switch (r) {
        case Radix::R8:  radixSort8(data, allowShortcuts);  break;
        case Radix::R11: radixSort11(data, allowShortcuts); break;
        case Radix::R16: radixSort16(data, allowShortcuts); break;
    }
}

// ============================================================================
// REAL QI-DRIVEN ADAPTIVE COST MODEL
//
// Incorporates Quantum-Inspired State Vector Observables:
//   - Amplitude Concentration IPR_b = sum(|psi_i|^4)
//   - Effective States N_eff = 1 / IPR
//   - Byte Entropy H_b
//   - Duplicate Ratio D
//   - Active Bit Width
//
// Rationale:
// Radix-16 uses 65,536 buckets (~512KB array).
// When key entropy is HIGH (high N_eff across bytes), scatter phase accesses
// buckets pseudo-randomly, thrashing CPU L2 cache (~256KB-512KB).
// When key entropy is LOW (concentrated amplitude IPR -> 1, low N_eff),
// scatter accesses only a tiny subset of buckets, staying entirely in L1/L2 cache!
//
// Radix-8 uses 256 buckets (2KB array), fitting in L1 cache regardless of entropy.
// Radix-11 uses 2,048 buckets (16KB array), fitting in L2 cache.
// ============================================================================

struct RadixPrediction {
    Radix radix;
    double cost;
    double confidence = 0.0;
};

static double estimateQI_RadixCost(const QIState& s, Radix radix, size_t n) {
    double bits = static_cast<double>(static_cast<int>(radix));
    
    // Active bit-width detection from bitwise OR/AND
    u32 activeMask = s.bitOrSum ^ s.bitAndSum;
    double activeBits = 32.0;
    if (activeMask <= 0xFFFFu) activeBits = 16.0;
    else if (activeMask <= 0xFFFFFFu) activeBits = 24.0;

    double passes = ceil(activeBits / bits);

    // Base pass cost
    double baseScanCost = passes * 1.0;

    // Cache thrashing & scatter cost model derived from QI State observables
    double cacheThrashingPenalty = 0.0;

    if (radix == Radix::R16) {
        // Radix-16 bucket size = 65,536.
        // Effective active buckets = product of effective states of active byte pairs
        double activeBuckets = min(65536.0, s.bytes[0].effectiveStates * s.bytes[1].effectiveStates);
        
        // High entropy H and low IPR (high N_eff) cause random scattering across 65K buckets
        double dispersionFactor = s.averageEntropy * (1.0 - s.amplitudeConcentration);
        
        // If active buckets exceed L2 cache capacity (~32K size_t elements = 256KB)
        double cacheOverflow = max(0.0, activeBuckets - 32768.0) / 32768.0;
        
        cacheThrashingPenalty = cacheOverflow * dispersionFactor * 2.8;
    }
    else if (radix == Radix::R11) {
        // Radix-11 bucket size = 2,048 (16KB size_t array, fits comfortably in L2)
        double activeBuckets = min(2048.0, s.effectiveStates * 8.0);
        double dispersionFactor = s.averageEntropy * (1.0 - s.amplitudeConcentration);
        cacheThrashingPenalty = dispersionFactor * 0.4;
    }
    else if (radix == Radix::R8) {
        // Radix-8 bucket size = 256 (2KB size_t array, fits in L1 cache)
        cacheThrashingPenalty = 0.05; // Minimal cache thrashing penalty
    }

    double totalCost = n * (baseScanCost + cacheThrashingPenalty);

    // Orderedness shortcut factor
    if (s.orderedness > 0.98) totalCost *= 0.05;
    
    // Duplicate ratio discount
    if (s.duplicateRatio > 0.80) totalCost *= (1.0 - 0.40 * s.duplicateRatio);

    return totalCost;
}

struct StrategyResult {
    Radix selected;
    double selectionTime = 0.0;
    double confidence = 0.0;
    double predictedCost = 0.0;
    bool usedQI = true;
};

static StrategyResult selectQIStrategy(const QIState& state, size_t n) {
    auto start = Clock::now();

    vector<RadixPrediction> predictions;
    for (Radix r : { Radix::R8, Radix::R11, Radix::R16 }) {
        predictions.push_back({ r, estimateQI_RadixCost(state, r, n), 0.0 });
    }
    sort(predictions.begin(), predictions.end(), [](const auto& a, const auto& b) {
        return a.cost < b.cost;
    });

    double best = predictions[0].cost;
    double second = predictions[1].cost;
    double confidence = (second > 0.0) ? (second - best) / second : 0.0;

    auto end = Clock::now();
    double selectionTime = chrono::duration<double, milli>(end - start).count();

    return { predictions[0].radix, selectionTime, confidence, best, true };
}

// ============================================================================
// CLASSICAL HEURISTIC ADAPTIVE SELECTOR (EXPLICITLY TIMED)
// ============================================================================

struct ClassicalStrategyResult {
    Radix selected;
    double analysisTime = 0.0;
};

static ClassicalStrategyResult selectClassicalStrategy(const vector<u32>& data) {
    auto start = Clock::now();

    size_t sampleSize = min<size_t>(STATE_SAMPLE_SIZE, data.size());
    array<array<u64, 256>, 4> counts{};
    size_t step = max<size_t>(1, data.size() / sampleSize);

    for (size_t i = 0; i < sampleSize; ++i) {
        size_t idx = (i * step);
        if (idx >= data.size()) idx = data.size() - 1;
        u32 x = data[idx];
        for (int b = 0; b < 4; ++b) counts[b][getByte(x, b)]++;
    }

    double occupied = 0.0;
    double entropySum = 0.0;

    for (int b = 0; b < 4; ++b) {
        int occ = 0;
        for (u64 c : counts[b]) if (c) occ++;
        occupied += occ;
        entropySum += entropy(counts[b], sampleSize);
    }
    occupied /= 4.0;
    entropySum /= 4.0;

    Radix selected = Radix::R16;
    if (occupied < 8.0) selected = Radix::R16;
    else if (entropySum > 0.85) selected = Radix::R8; // Classical heuristic switches to R8 on high entropy
    else if (entropySum > 0.50) selected = Radix::R11;
    else selected = Radix::R16;

    auto end = Clock::now();
    double analysisTime = chrono::duration<double, milli>(end - start).count();

    return { selected, analysisTime };
}

// ============================================================================
// BENCHMARKING PIPELINE & BASELINES
// ============================================================================

static Timing benchmarkRadix(const vector<u32>& input, Radix radix, bool allowShortcuts = true) {
    vector<double> times;
    times.reserve(TIMING_TRIALS);

    for (int trial = 0; trial < TIMING_TRIALS; ++trial) {
        vector<u32> data = input;
        auto start = Clock::now();
        radixSort(data, radix, allowShortcuts);
        auto end = Clock::now();
        volatile u32 guard = data[data.size() / 2]; (void)guard;
        times.push_back(chrono::duration<double, milli>(end - start).count());
    }
    return summarize(times);
}

static Timing benchmarkStdSort(const vector<u32>& input) {
    vector<double> times;
    times.reserve(TIMING_TRIALS);

    for (int trial = 0; trial < TIMING_TRIALS; ++trial) {
        vector<u32> data = input;
        auto start = Clock::now();
        sort(data.begin(), data.end());
        auto end = Clock::now();
        volatile u32 guard = data[data.size() / 2]; (void)guard;
        times.push_back(chrono::duration<double, milli>(end - start).count());
    }
    return summarize(times);
}

static Timing benchmarkStdStableSort(const vector<u32>& input) {
    vector<double> times;
    times.reserve(TIMING_TRIALS);

    for (int trial = 0; trial < TIMING_TRIALS; ++trial) {
        vector<u32> data = input;
        auto start = Clock::now();
        stable_sort(data.begin(), data.end());
        auto end = Clock::now();
        volatile u32 guard = data[data.size() / 2]; (void)guard;
        times.push_back(chrono::duration<double, milli>(end - start).count());
    }
    return summarize(times);
}

static bool checkCorrectness(const vector<u32>& input, Radix radix) {
    vector<u32> reference = input;
    vector<u32> test = input;
    sort(reference.begin(), reference.end());
    radixSort(test, radix);
    return reference == test;
}

struct ExperimentResult {
    string dataset;
    Radix qiRadix;
    Radix classicalRadix;

    Timing stdSort;
    Timing stdStableSort;
    Timing radix8;
    Timing radix11;
    Timing radix16;

    // Ablation baselines
    Timing pureRadix16; // Radix-16 without O(N) early exit shortcuts

    Timing classicalSort;
    Timing qiSort;

    double qiStateTime = 0.0;
    double qiSelectionTime = 0.0;
    double qiTotalTime = 0.0;

    double classicalAnalysisTime = 0.0;
    double classicalTotalTime = 0.0;

    bool correctness = false;
    bool qiBeatsClassicalSort = false;
    bool qiBeatsClassicalTotal = false;
};

static double speedup(double baseline, double method) {
    if (method <= 0.0) return 0.0;
    return baseline / method;
}

static ExperimentResult runExperiment(DatasetType type, const vector<u32>& input) {
    ExperimentResult result;
    result.dataset = datasetName(type);

    // QI Pipeline Timing
    QIState state = constructQIState(input);
    result.qiStateTime = state.analysisTime;

    StrategyResult qi = selectQIStrategy(state, input.size());
    result.qiRadix = qi.selected;
    result.qiSelectionTime = qi.selectionTime;

    // Classical Adaptive Pipeline Timing
    ClassicalStrategyResult classicalRes = selectClassicalStrategy(input);
    result.classicalRadix = classicalRes.selected;
    result.classicalAnalysisTime = classicalRes.analysisTime;

    // Benchmarks
    result.stdSort = benchmarkStdSort(input);
    result.stdStableSort = benchmarkStdStableSort(input);
    result.radix8 = benchmarkRadix(input, Radix::R8);
    result.radix11 = benchmarkRadix(input, Radix::R11);
    result.radix16 = benchmarkRadix(input, Radix::R16);
    result.pureRadix16 = benchmarkRadix(input, Radix::R16, false); // No shortcuts

    result.classicalSort = benchmarkRadix(input, result.classicalRadix);
    result.qiSort = benchmarkRadix(input, result.qiRadix);

    // Total Pipeline Costs Including Analysis Overhead
    result.qiTotalTime = result.qiStateTime + result.qiSelectionTime + result.qiSort.median;
    result.classicalTotalTime = result.classicalAnalysisTime + result.classicalSort.median;

    result.correctness = checkCorrectness(input, result.qiRadix);

    // Fair Win Metrics
    result.qiBeatsClassicalSort = (result.qiSort.median <= result.classicalSort.median);
    result.qiBeatsClassicalTotal = (result.qiTotalTime <= result.classicalTotalTime);

    return result;
}

// ============================================================================
// MAIN EXPERIMENT RUNNER & DUAL METRIC REPORTING
// ============================================================================

int main() {
    ios::sync_with_stdio(false);

    cout << "============================================================\n";
    cout << "QI-RADIX: PROBABILITY-AMPLITUDE-INSPIRED ADAPTIVE RADIX SORT\n";
    cout << "============================================================\n\n";

    vector<DatasetType> datasets = {
        DatasetType::RANDOM,
        DatasetType::DUPLICATE_HEAVY,
        DatasetType::CLUSTERED,
        DatasetType::SORTED,
        DatasetType::REVERSE,
        DatasetType::LOW_RANGE,
        DatasetType::ALTERNATING,
        DatasetType::ALMOST_SORTED,
        DatasetType::POWER_DISTRIBUTION,
        DatasetType::SAWTOOTH
    };

    vector<ExperimentResult> results;
    results.reserve(datasets.size());
    uint64_t seedBase = 0xC0FFEE123456789ULL;

    for (size_t i = 0; i < datasets.size(); ++i) {
        uint64_t seed = seedBase + i * 100003;
        vector<u32> data = generateDataset(datasets[i], N, seed);
        ExperimentResult result = runExperiment(datasets[i], data);
        results.push_back(result);
    }

    size_t correctnessFailures = 0;
    size_t qiSortWins = 0;
    size_t qiTotalWins = 0;

    double sumQI_TotalTime = 0.0;
    double sumClassical_TotalTime = 0.0;
    double sumStdSortTime = 0.0;
    double sumStdStableSortTime = 0.0;
    double sumPureRadix16Time = 0.0;

    double sumPerDatasetSpeedupVsStd = 0.0;
    double sumPerDatasetSpeedupVsClass = 0.0;

    for (const auto& r : results) {
        if (!r.correctness) correctnessFailures++;
        if (r.qiBeatsClassicalSort) qiSortWins++;
        if (r.qiBeatsClassicalTotal) qiTotalWins++;

        sumQI_TotalTime += r.qiTotalTime;
        sumClassical_TotalTime += r.classicalTotalTime;
        sumStdSortTime += r.stdSort.median;
        sumStdStableSortTime += r.stdStableSort.median;
        sumPureRadix16Time += r.pureRadix16.median;

        sumPerDatasetSpeedupVsStd += speedup(r.stdSort.median, r.qiTotalTime);
        sumPerDatasetSpeedupVsClass += speedup(r.classicalTotalTime, r.qiTotalTime);
    }

    double count = static_cast<double>(results.size());

    cout << "TABLE I: DETAILED PIPELINE BENCHMARK (N=1,000,000, 7-Trial Median in ms)\n\n";
    cout << left << setw(20) << "Dataset"
         << setw(12) << "QI Radix"
         << setw(12) << "Class Radix"
         << setw(12) << "std::sort"
         << setw(12) << "Pure R16"
         << setw(14) << "QI Sort Only"
         << setw(14) << "QI Total"
         << setw(14) << "Class Total"
         << setw(9)  << "Correct" << "\n";
    cout << "---------------------------------------------------------------------------------------------------------------\n";

    for (const auto& r : results) {
        cout << left << setw(20) << r.dataset
             << setw(12) << radixName(r.qiRadix)
             << setw(12) << radixName(r.classicalRadix)
             << setw(12) << fixed << setprecision(3) << r.stdSort.median
             << setw(12) << r.pureRadix16.median
             << setw(14) << r.qiSort.median
             << setw(14) << r.qiTotalTime
             << setw(14) << r.classicalTotalTime
             << setw(9)  << (r.correctness ? "PASS" : "FAIL") << "\n";
    }

    // Aggregate Calculations
    double meanPerDatasetSpeedupVsStd = sumPerDatasetSpeedupVsStd / count;
    double aggregateSpeedupVsStd = sumStdSortTime / sumQI_TotalTime;

    double meanPerDatasetSpeedupVsClass = sumPerDatasetSpeedupVsClass / count;
    double aggregateSpeedupVsClass = sumClassical_TotalTime / sumQI_TotalTime;

    cout << "\n============================================================\n";
    cout << "SCIENTIFIC EXPERIMENT SUMMARY & DUAL METRIC ANALYSIS\n";
    cout << "============================================================\n";
    cout << "Datasets tested                      : " << results.size() << "\n";
    cout << "Correctness failures                : " << correctnessFailures << "\n";
    cout << "QI Strategy Selection Breakdown      :\n";
    
    size_t countR8 = 0, countR11 = 0, countR16 = 0;
    for (const auto& r : results) {
        if (r.qiRadix == Radix::R8) countR8++;
        else if (r.qiRadix == Radix::R11) countR11++;
        else if (r.qiRadix == Radix::R16) countR16++;
    }
    cout << "  - Radix-8 selected                 : " << countR8 << " / 10\n";
    cout << "  - Radix-11 selected                : " << countR11 << " / 10\n";
    cout << "  - Radix-16 selected                : " << countR16 << " / 10\n\n";

    cout << "QI Sort-Only Wins vs Classical Baseline : " << qiSortWins << " / " << results.size() << "\n";
    cout << "QI Total Pipeline Wins vs Class Baseline: " << qiTotalWins << " / " << results.size() << "\n\n";

    cout << "Aggregate Time Summary across 10 Datasets:\n";
    cout << "  - Total std::sort time             : " << sumStdSortTime << " ms\n";
    cout << "  - Total Pure Radix-16 time         : " << sumPureRadix16Time << " ms\n";
    cout << "  - Total Classical Baseline time    : " << sumClassical_TotalTime << " ms\n";
    cout << "  - Total QI-Radix Pipeline time     : " << sumQI_TotalTime << " ms\n\n";

    cout << "Dual Metric Speedup Breakdown:\n";
    cout << "  - Mean Per-Dataset Speedup vs std::sort  : " << meanPerDatasetSpeedupVsStd << "x\n";
    cout << "  - Aggregate Benchmark Speedup vs std::sort: " << aggregateSpeedupVsStd << "x\n\n";
    cout << "  - Mean Per-Dataset Speedup vs Class Base : " << meanPerDatasetSpeedupVsClass << "x\n";
    cout << "  - Aggregate Benchmark Speedup vs Class Base: " << aggregateSpeedupVsClass << "x\n";
    cout << "============================================================\n";

    return correctnessFailures == 0 ? 0 : 1;
}
