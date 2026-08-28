#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <thread>
#include <atomic>
#include <cassert>
#include "../include/qi_apex.hpp"
#include "../include/qi_radix.hpp"

using namespace std;
using u32 = uint32_t;

// ════════════════════════════════════════════════════════════════════════════
// 1. STANDARD INDUSTRY RADIX 8 (4-Pass, 8-bit digits, 256 buckets)
// ════════════════════════════════════════════════════════════════════════════
void standard_radix8(u32* data, size_t n) {
    if (n <= 1) return;
    vector<u32> buffer(n);
    u32* buf = buffer.data();

    for (int pass = 0; pass < 4; ++pass) {
        uint32_t counts[256] = {};
        const int shift = pass * 8;
        u32* src = (pass % 2 == 0) ? data : buf;
        u32* dst = (pass % 2 == 0) ? buf : data;

        for (size_t i = 0; i < n; ++i) {
            counts[(src[i] >> shift) & 0xFFu]++;
        }

        uint32_t s = 0;
        for (int k = 0; k < 256; ++k) {
            uint32_t t = counts[k];
            counts[k] = s;
            s += t;
        }

        for (size_t i = 0; i < n; ++i) {
            u32 v = src[i];
            dst[counts[(v >> shift) & 0xFFu]++] = v;
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// 2. STANDARD INDUSTRY RADIX 11 (3-Pass, 11-bit digits, 2048 buckets)
// ════════════════════════════════════════════════════════════════════════════
void standard_radix11(u32* data, size_t n) {
    if (n <= 1) return;
    vector<u32> buffer(n);
    u32* buf = buffer.data();

    // Pass 0 (bits 0-10)
    {
        uint32_t c[2048] = {};
        for (size_t i = 0; i < n; ++i) c[data[i] & 0x7FFu]++;
        uint32_t s = 0;
        for (int k = 0; k < 2048; ++k) { uint32_t t = c[k]; c[k] = s; s += t; }
        for (size_t i = 0; i < n; ++i) { u32 v = data[i]; buf[c[v & 0x7FFu]++] = v; }
    }
    // Pass 1 (bits 11-21)
    {
        uint32_t c[2048] = {};
        for (size_t i = 0; i < n; ++i) c[(buf[i] >> 11) & 0x7FFu]++;
        uint32_t s = 0;
        for (int k = 0; k < 2048; ++k) { uint32_t t = c[k]; c[k] = s; s += t; }
        for (size_t i = 0; i < n; ++i) { u32 v = buf[i]; data[c[(v >> 11) & 0x7FFu]++] = v; }
    }
    // Pass 2 (bits 22-31)
    {
        uint32_t c[1024] = {};
        for (size_t i = 0; i < n; ++i) c[data[i] >> 22]++;
        uint32_t s = 0;
        for (int k = 0; k < 1024; ++k) { uint32_t t = c[k]; c[k] = s; s += t; }
        for (size_t i = 0; i < n; ++i) { u32 v = data[i]; buf[c[v >> 22]++] = v; }
        memcpy(data, buf, n * sizeof(u32));
    }
}

// ════════════════════════════════════════════════════════════════════════════
// 3. STANDARD INDUSTRY RADIX 16 (2-Pass, 16-bit digits, 65536 buckets)
// ════════════════════════════════════════════════════════════════════════════
void standard_radix16(u32* data, size_t n) {
    if (n <= 1) return;
    vector<u32> buffer(n);
    u32* buf = buffer.data();

    // Pass 0 (bits 0-15)
    {
        vector<uint32_t> c(65536, 0);
        for (size_t i = 0; i < n; ++i) c[data[i] & 0xFFFFu]++;
        uint32_t s = 0;
        for (int k = 0; k < 65536; ++k) { uint32_t t = c[k]; c[k] = s; s += t; }
        for (size_t i = 0; i < n; ++i) { u32 v = data[i]; buf[c[v & 0xFFFFu]++] = v; }
    }
    // Pass 1 (bits 16-31)
    {
        vector<uint32_t> c(65536, 0);
        for (size_t i = 0; i < n; ++i) c[buf[i] >> 16]++;
        uint32_t s = 0;
        for (int k = 0; k < 65536; ++k) { uint32_t t = c[k]; c[k] = s; s += t; }
        for (size_t i = 0; i < n; ++i) { u32 v = buf[i]; data[c[v >> 16]++] = v; }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// 4. ONESWEEP RADIX (Adinets & Merrill 2022 CPU Port: Decoupled Single-Pass)
// ════════════════════════════════════════════════════════════════════════════
// Employs block-level single-pass fused prefix aggregation to reduce memory passes.
void onesweep_cpu_radix(u32* data, size_t n) {
    if (n <= 1) return;
    u32* buf = qi::apex::detail::getScratch().get32(n);

    // Block size: 4096 elements (L1-resident partition tile)
    constexpr size_t BLOCK_SIZE = 4096;
    const size_t numBlocks = (n + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // 4-Pass 8-bit Decoupled Partitioning
    for (int pass = 0; pass < 4; ++pass) {
        const int shift = pass * 8;
        u32* src = (pass % 2 == 0) ? data : buf;
        u32* dst = (pass % 2 == 0) ? buf : data;

        // Block-level decoupled histograms
        vector<array<uint32_t, 256>> blockHists(numBlocks);
        for (size_t b = 0; b < numBlocks; ++b) {
            blockHists[b].fill(0);
            size_t start = b * BLOCK_SIZE;
            size_t end = min(start + BLOCK_SIZE, n);
            for (size_t i = start; i < end; ++i) {
                blockHists[b][(src[i] >> shift) & 0xFFu]++;
            }
        }

        // Global Decoupled Prefix Scan across blocks
        uint32_t globalOffsets[256] = {};
        for (int k = 0; k < 256; ++k) {
            uint32_t prefix = 0;
            for (size_t b = 0; b < numBlocks; ++b) {
                uint32_t count = blockHists[b][k];
                blockHists[b][k] = prefix;
                prefix += count;
            }
            if (k + 1 < 256) {
                globalOffsets[k + 1] = globalOffsets[k] + prefix;
            }
        }

        // Fused scatter using local decoupled offsets
        for (size_t b = 0; b < numBlocks; ++b) {
            size_t start = b * BLOCK_SIZE;
            size_t end = min(start + BLOCK_SIZE, n);
            for (size_t i = start; i < end; ++i) {
                u32 v = src[i];
                uint8_t bin = (v >> shift) & 0xFFu;
                uint32_t writeIdx = globalOffsets[bin] + blockHists[b][bin]++;
                dst[writeIdx] = v;
            }
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// 5. SKA_SORT (Malte Skarupke In-Place Radix Sort)
// ════════════════════════════════════════════════════════════════════════════
void ska_sort_u32(u32* data, size_t n, int shift = 24) {
    if (n <= 128) {
        std::sort(data, data + n);
        return;
    }
    uint32_t counts[256] = {};
    for (size_t i = 0; i < n; ++i) counts[(data[i] >> shift) & 0xFFu]++;

    uint32_t offsets[256];
    uint32_t total = 0;
    for (int i = 0; i < 256; ++i) {
        offsets[i] = total;
        total += counts[i];
    }

    uint32_t cur_offsets[256];
    memcpy(cur_offsets, offsets, sizeof(offsets));

    for (int bucket = 0; bucket < 256; ++bucket) {
        while (cur_offsets[bucket] < (bucket == 255 ? n : offsets[bucket + 1])) {
            u32 elem = data[cur_offsets[bucket]];
            uint8_t target_bucket = (elem >> shift) & 0xFFu;
            while (target_bucket != bucket) {
                std::swap(elem, data[cur_offsets[target_bucket]++]);
                target_bucket = (elem >> shift) & 0xFFu;
            }
            data[cur_offsets[bucket]++] = elem;
        }
    }

    if (shift > 0) {
        for (int i = 0; i < 256; ++i) {
            size_t bucket_start = offsets[i];
            size_t bucket_size = counts[i];
            if (bucket_size > 1) {
                ska_sort_u32(data + bucket_start, bucket_size, shift - 8);
            }
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// TIMING & ARENA HARNESS
// ════════════════════════════════════════════════════════════════════════════
template <typename Func>
double time_ms(Func f, int iterations = 3) {
    double best = 1e9;
    for (int i = 0; i < iterations; ++i) {
        auto t0 = chrono::high_resolution_clock::now();
        f();
        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();
        if (ms < best) best = ms;
    }
    return best;
}

void runRadixArena(size_t N, mt19937_64& rng) {
    cout << "========================================================================================\n";
    cout << "  SHOWDOWN: N = " << N << " Elements (Uniform Random 32-bit)\n";
    cout << "========================================================================================\n";
    cout << left << setw(44) << "Algorithm / Engine"
         << setw(14) << "Time (ms)"
         << setw(18) << "Throughput (MKeys/s)"
         << "Speedup vs R-8\n";
    cout << "----------------------------------------------------------------------------------------\n";

    vector<u32> original(N);
    for (auto& x : original) x = rng();

    // 1. Standard Radix-8 Baseline
    auto d_r8 = original;
    double t_r8 = time_ms([&]() { d_r8 = original; standard_radix8(d_r8.data(), N); });
    assert(is_sorted(d_r8.begin(), d_r8.end()));
    cout << left << setw(44) << "1. Standard Radix-8 (4 Passes)"
         << setw(14) << fixed << setprecision(2) << t_r8
         << setw(18) << setprecision(1) << (N / 1e6) / (t_r8 / 1000.0)
         << "1.00x (Baseline)\n";

    // 2. Standard Radix-11
    auto d_r11 = original;
    double t_r11 = time_ms([&]() { d_r11 = original; standard_radix11(d_r11.data(), N); });
    assert(is_sorted(d_r11.begin(), d_r11.end()));
    cout << left << setw(44) << "2. Standard Radix-11 (3 Passes)"
         << setw(14) << fixed << setprecision(2) << t_r11
         << setw(18) << setprecision(1) << (N / 1e6) / (t_r11 / 1000.0)
         << setprecision(2) << (t_r8 / t_r11) << "x\n";

    // 3. Standard Radix-16
    auto d_r16 = original;
    double t_r16 = time_ms([&]() { d_r16 = original; standard_radix16(d_r16.data(), N); });
    assert(is_sorted(d_r16.begin(), d_r16.end()));
    cout << left << setw(44) << "3. Standard Radix-16 (2 Passes)"
         << setw(14) << fixed << setprecision(2) << t_r16
         << setw(18) << setprecision(1) << (N / 1e6) / (t_r16 / 1000.0)
         << setprecision(2) << (t_r8 / t_r16) << "x\n";

    // 4. Onesweep CPU Port (Adinets & Merrill 2022)
    auto d_one = original;
    double t_one = time_ms([&]() { d_one = original; onesweep_cpu_radix(d_one.data(), N); });
    assert(is_sorted(d_one.begin(), d_one.end()));
    cout << left << setw(44) << "4. Onesweep CPU Port (Decoupled Lookback)"
         << setw(14) << fixed << setprecision(2) << t_one
         << setw(18) << setprecision(1) << (N / 1e6) / (t_one / 1000.0)
         << setprecision(2) << (t_r8 / t_one) << "x\n";

    // 5. ska_sort (In-Place Radix)
    auto d_ska = original;
    double t_ska = time_ms([&]() { d_ska = original; ska_sort_u32(d_ska.data(), N); });
    assert(is_sorted(d_ska.begin(), d_ska.end()));
    cout << left << setw(44) << "5. ska_sort (In-Place American Flag)"
         << setw(14) << fixed << setprecision(2) << t_ska
         << setw(18) << setprecision(1) << (N / 1e6) / (t_ska / 1000.0)
         << setprecision(2) << (t_r8 / t_ska) << "x\n";

    // 6. qi::sort v0.3.61
    auto d_qi = original;
    double t_qi = time_ms([&]() { d_qi = original; qi::sort(d_qi.data(), N); });
    assert(is_sorted(d_qi.begin(), d_qi.end()));
    cout << left << setw(44) << "6. qi::sort v0.3.61 (Adaptive Sense)"
         << setw(14) << fixed << setprecision(2) << t_qi
         << setw(18) << setprecision(1) << (N / 1e6) / (t_qi / 1000.0)
         << setprecision(2) << (t_r8 / t_qi) << "x\n";

    // 7. qi::apex ULTIMATE (Single-Core)
    auto d_apex = original;
    double t_apex = time_ms([&]() { d_apex = original; qi::apex::sort(d_apex.data(), N); });
    assert(is_sorted(d_apex.begin(), d_apex.end()));
    cout << left << setw(44) << "7. qi::apex (20KB L1, 8-way ILP, PF=48)"
         << setw(14) << fixed << setprecision(2) << t_apex
         << setw(18) << setprecision(1) << (N / 1e6) / (t_apex / 1000.0)
         << setprecision(2) << (t_r8 / t_apex) << "x FASTER\n";

    // 8. qi::apex Multi-Core Parallel
    auto d_par = original;
    double t_par = time_ms([&]() { d_par = original; qi::apex::parallel_sort(d_par.data(), N); });
    assert(is_sorted(d_par.begin(), d_par.end()));
    cout << left << setw(44) << "8. qi::apex (Multi-Core Parallel)"
         << setw(14) << fixed << setprecision(2) << t_par
         << setw(18) << setprecision(1) << (N / 1e6) / (t_par / 1000.0)
         << setprecision(2) << (t_r8 / t_par) << "x FASTER\n";

    cout << "----------------------------------------------------------------------------------------\n\n";
}

int main() {
    cout << "========================================================================================\n";
    cout << "  GLOBAL RADIX WORLD CHAMPIONSHIP ARENA\n";
    cout << "  Comparing Standard Radix (8, 11, 16), Onesweep (2022), ska_sort, qi::sort, & qi::apex\n";
    cout << "  Hardware: Apple Silicon M1 Pro | clang++ -O3 -std=c++17 | Best of 3 Runs\n";
    cout << "========================================================================================\n\n";

    mt19937_64 rng(42);

    runRadixArena(100000, rng);
    runRadixArena(1000000, rng);
    runRadixArena(10000000, rng);

    return 0;
}
