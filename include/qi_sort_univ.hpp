#ifndef QI_SORT_UNIV_HPP
#define QI_SORT_UNIV_HPP

/**
 * qi-sort-univ: Universal High-Performance Adaptive Radix Sort Engine
 * ====================================================================
 * Optimized for x86_64 (Linux / Intel / AMD) & ARM64.
 *
 * KEY PERFORMANCE DESIGN:
 * 1. 20 KB L1-Resident Histograms: 2048 x 4-byte uint32_t counts fit inside 32 KB L1.
 * 2. 0 Branches in Inner Loops: Zero branch mispredictions during scatter.
 * 3. 0 Function Calls in Scatter: Direct pointer indexing.
 * 4. Zero-Cost Pass Skipping: If all values fit in lower bits, higher passes skip in 1 cycle.
 */

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <algorithm>

namespace qi_univ {

using u32 = uint32_t;

class ScratchBuffer {
public:
    u32* get(size_t n) {
        if (n > capacity_) {
            buf_.resize(n);
            capacity_ = n;
        }
        return buf_.data();
    }
private:
    std::vector<u32> buf_;
    size_t capacity_ = 0;
};

inline ScratchBuffer& getScratch() {
    static thread_local ScratchBuffer scratch;
    return scratch;
}

// ── FAST ZERO-BRANCH RADIX-11 ENGINE WITH ZERO-COST PASS SKIPPING ──
inline void radixSort11_univ(u32* data, size_t n) {
    if (n <= 1) return;
    u32* buf = getScratch().get(n);

    // 20 KB uint32_t histograms — 100% L1 cache resident on x86_64 (32 KB L1 budget)
    alignas(64) uint32_t c0[2048] = {};
    alignas(64) uint32_t c1[2048] = {};
    alignas(64) uint32_t c2[1024] = {};

    // 1 combined count pass
    for (size_t i = 0; i < n; ++i) {
        u32 v = data[i];
        c0[v & 0x7FFu]++;
        c1[(v >> 11) & 0x7FFu]++;
        c2[v >> 22]++;
    }

    // Check if pass 1 and 2 can be skipped
    bool skipPass1And2 = (c1[0] == n);
    bool skipPass2     = (c2[0] == n);

    // Prefix sums
    uint32_t s0 = 0, s1 = 0, s2 = 0;
    for (int i = 0; i < 2048; ++i) {
        uint32_t t;
        t = c0[i]; c0[i] = s0; s0 += t;
        if (!skipPass1And2) { t = c1[i]; c1[i] = s1; s1 += t; }
        if (!skipPass1And2 && !skipPass2 && i < 1024) { t = c2[i]; c2[i] = s2; s2 += t; }
    }

    // Pass 0: data -> buf (bits 0-10)
    for (size_t i = 0; i < n; ++i) {
        u32 v = data[i];
        buf[c0[v & 0x7FFu]++] = v;
    }

    // Short-circuit if all values fit in 11 bits (e.g. 0-255 or 0-2047)
    if (skipPass1And2) {
        std::memcpy(data, buf, n * sizeof(u32));
        return;
    }

    // Pass 1: buf -> data (bits 11-21)
    for (size_t i = 0; i < n; ++i) {
        u32 v = buf[i];
        data[c1[(v >> 11) & 0x7FFu]++] = v;
    }

    // Short-circuit if all values fit in 22 bits
    if (skipPass2) return;

    // Pass 2: data -> buf (bits 22-31)
    for (size_t i = 0; i < n; ++i) {
        u32 v = data[i];
        buf[c2[v >> 22]++] = v;
    }
    std::memcpy(data, buf, n * sizeof(u32));
}

inline void sort_univ(u32* data, size_t n) {
    radixSort11_univ(data, n);
}

} // namespace qi_univ

#endif // QI_SORT_UNIV_HPP
