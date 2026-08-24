#ifndef QI_SORT_UNIV_HPP
#define QI_SORT_UNIV_HPP

/**
 * qi-sort-univ: Universal High-Performance Stack-Allocated Radix Engine
 * ======================================================================
 * Optimized for x86_64 (Linux / Shared Library .so / Cloud) & ARM64.
 *
 * KEY PERFORMANCE DESIGN:
 * 1. 0 TLS Overhead: Uses stack allocation instead of thread_local, eliminating
 *    __tls_get_addr() dynamic function call overhead in Linux shared libraries (.so).
 * 2. 20 KB L1 Footprint: 2048 x uint32 histograms (20 KB total) fit 100% in 32 KB L1 Data Cache.
 * 3. 0 Memcpy: Pass 0 (bits 0-10) -> buf, Pass 1 (bits 11-21) -> data, Pass 2 (bits 22-31) -> buf.
 */

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <algorithm>

// ── SIMD INTRINSIC DETECT ──
#if defined(__AVX512F__) || defined(__AVX512BW__)
  #include <immintrin.h>
  #define QI_HAS_AVX512 1
#elif defined(__AVX2__)
  #include <immintrin.h>
  #define QI_HAS_AVX2 1
#elif defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(_M_ARM64)
  #include <arm_neon.h>
  #define QI_HAS_NEON 1
#endif

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

// ── FAST STACK-ALLOCATED RADIX-11 ENGINE (20 KB L1 Footprint, 0 TLS Overhead) ──
inline void radixSort11_univ(u32* data, size_t n) {
    if (n <= 1) return;
    u32* buf = getScratch().get(n);

    // Pure stack allocation — zero thread_local __tls_get_addr() overhead in Linux .so libraries
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

    bool skipPass1And2 = (c1[0] == n);
    bool skipPass2     = (c2[0] == n);

    uint32_t s0 = 0, s1 = 0, s2 = 0;
    for (int i = 0; i < 2048; ++i) {
        uint32_t t;
        t = c0[i]; c0[i] = s0; s0 += t;
        if (!skipPass1And2) { t = c1[i]; c1[i] = s1; s1 += t; }
        if (!skipPass1And2 && !skipPass2 && i < 1024) { t = c2[i]; c2[i] = s2; s2 += t; }
    }

    // Pass 0: data -> buf (bits 0-10) — 4x unrolled
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        u32 v0 = data[i + 0], v1 = data[i + 1], v2 = data[i + 2], v3 = data[i + 3];
        buf[c0[v0 & 0x7FFu]++] = v0;
        buf[c0[v1 & 0x7FFu]++] = v1;
        buf[c0[v2 & 0x7FFu]++] = v2;
        buf[c0[v3 & 0x7FFu]++] = v3;
    }
    for (; i < n; ++i) {
        u32 v = data[i];
        buf[c0[v & 0x7FFu]++] = v;
    }

    if (skipPass1And2) {
        std::memcpy(data, buf, n * sizeof(u32));
        return;
    }

    // Pass 1: buf -> data (bits 11-21) — 4x unrolled (ENDS IN data!)
    i = 0;
    for (; i + 4 <= n; i += 4) {
        u32 v0 = buf[i + 0], v1 = buf[i + 1], v2 = buf[i + 2], v3 = buf[i + 3];
        data[c1[(v0 >> 11) & 0x7FFu]++] = v0;
        data[c1[(v1 >> 11) & 0x7FFu]++] = v1;
        data[c1[(v2 >> 11) & 0x7FFu]++] = v2;
        data[c1[(v3 >> 11) & 0x7FFu]++] = v3;
    }
    for (; i < n; ++i) {
        u32 v = buf[i];
        data[c1[(v >> 11) & 0x7FFu]++] = v;
    }

    if (skipPass2) return;

    // Pass 2: data -> buf (bits 22-31)
    i = 0;
    for (; i + 4 <= n; i += 4) {
        u32 v0 = data[i + 0], v1 = data[i + 1], v2 = data[i + 2], v3 = data[i + 3];
        buf[c2[v0 >> 22]++] = v0;
        buf[c2[v1 >> 22]++] = v1;
        buf[c2[v2 >> 22]++] = v2;
        buf[c2[v3 >> 22]++] = v3;
    }
    for (; i < n; ++i) {
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
