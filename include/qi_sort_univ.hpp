#ifndef QI_SORT_UNIV_HPP
#define QI_SORT_UNIV_HPP

/**
 * qi-sort-univ: Universal Block-Buffered Adaptive Radix Sort Engine
 * =================================================================
 * Optimized for x86_64 (Linux/Intel/AMD) and ARM64 architectures.
 * Uses 32-byte cache-line write buffering to eliminate DRAM line-fill
 * stalls during non-contiguous bucket scatter passes.
 */

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <algorithm>

namespace qi_univ {

using u32 = uint32_t;

// ── Scratch Buffer Management ──
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

// ── Universal Block-Buffered Radix-11 Engine ──
inline void radixSort11_univ(u32* data, size_t n, u32 bitOr = ~0u) {
    if (n <= 1) return;
    u32* buf = getScratch().get(n);

    // 20 KB uint32_t histograms — guaranteed L1 cache fit (32 KB L1 on x86_64)
    alignas(64) uint32_t c0[2048] = {};
    alignas(64) uint32_t c1[2048] = {};
    alignas(64) uint32_t c2[1024] = {};

    // Unconditional combined counting pass — fully SIMD vectorizable
    for (size_t i = 0; i < n; ++i) {
        u32 v = data[i];
        c0[v & 0x7FFu]++;
        c1[(v >> 11) & 0x7FFu]++;
        c2[v >> 22]++;
    }

    // Prefix sums
    uint32_t s0 = 0, s1 = 0, s2 = 0;
    for (int i = 0; i < 2048; ++i) {
        uint32_t t;
        t = c0[i]; c0[i] = s0; s0 += t;
        t = c1[i]; c1[i] = s1; s1 += t;
        if (i < 1024) { t = c2[i]; c2[i] = s2; s2 += t; }
    }

    // ── BLOCK-BUFFERED WRITE SCATTER ──
    // 8 elements (32 bytes) per buffer → 2048 × 32 = 64 KB total write buffer.
    // 87.5% of writes stay in fast L1 cache; 12.5% of writes are 32-byte block writes.
    alignas(64) static thread_local u32 wbuf[2048][8];
    alignas(64) static thread_local uint8_t wcnt[2048];

    // Pass 0: data → buf (bits 0-10)
    std::memset(wcnt, 0, 2048);
    for (size_t i = 0; i < n; ++i) {
        u32 v = data[i];
        u32 b = v & 0x7FFu;
        wbuf[b][wcnt[b]++] = v;
        if (wcnt[b] == 8) {
            std::memcpy(&buf[c0[b]], wbuf[b], 32);
            c0[b] += 8;
            wcnt[b] = 0;
        }
    }
    for (int b = 0; b < 2048; ++b) {
        if (wcnt[b] > 0) {
            std::memcpy(&buf[c0[b]], wbuf[b], wcnt[b] * sizeof(u32));
            c0[b] += wcnt[b];
            wcnt[b] = 0;
        }
    }

    // Pass 1: buf → data (bits 11-21)
    for (size_t i = 0; i < n; ++i) {
        u32 v = buf[i];
        u32 b = (v >> 11) & 0x7FFu;
        wbuf[b][wcnt[b]++] = v;
        if (wcnt[b] == 8) {
            std::memcpy(&data[c1[b]], wbuf[b], 32);
            c1[b] += 8;
            wcnt[b] = 0;
        }
    }
    for (int b = 0; b < 2048; ++b) {
        if (wcnt[b] > 0) {
            std::memcpy(&data[c1[b]], wbuf[b], wcnt[b] * sizeof(u32));
            c1[b] += wcnt[b];
            wcnt[b] = 0;
        }
    }

    // Pass 2 (optional): data → buf (bits 22-31)
    if ((bitOr >> 22) != 0) {
        for (size_t i = 0; i < n; ++i) {
            u32 v = data[i];
            u32 b = v >> 22;
            wbuf[b][wcnt[b]++] = v;
            if (wcnt[b] == 8) {
                std::memcpy(&buf[c2[b]], wbuf[b], 32);
                c2[b] += 8;
                wcnt[b] = 0;
            }
        }
        for (int b = 0; b < 1024; ++b) {
            if (wcnt[b] > 0) {
                std::memcpy(&buf[c2[b]], wbuf[b], wcnt[b] * sizeof(u32));
                c2[b] += wcnt[b];
                wcnt[b] = 0;
            }
        }
        std::memcpy(data, buf, n * sizeof(u32));
    }
}

inline void sort_univ(u32* data, size_t n) {
    radixSort11_univ(data, n);
}

} // namespace qi_univ

#endif // QI_SORT_UNIV_HPP
