#ifndef QI_SORT_UNIV_HPP
#define QI_SORT_UNIV_HPP

/**
 * qi-sort-univ: Universal Block-Buffered Adaptive Radix Sort Engine
 * =================================================================
 * Optimized specifically for x86_64 (Linux / Intel / AMD) & ARM64.
 * Uses a strict 5.25 KB working memory footprint (Radix-8 + 4-element write buffers)
 * guaranteeing 100% L1 Data Cache residency on ALL x86_64 CPUs (32 KB L1 budget).
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

// ── L1-RESIDENT RADIX-8 ENGINE (5.25 KB Working Footprint) ──
inline void radixSort8_univ(u32* data, size_t n, u32 bitOr = ~0u) {
    if (n <= 1) return;
    u32* buf = getScratch().get(n);
    u32* src = data;
    u32* dst = buf;

    // 4-element write buffer: 256 * 16 bytes = 4 KB (guaranteed L1 resident)
    alignas(64) u32 wbuf[256][4];
    alignas(64) uint8_t wcnt[256];

    for (int pass = 0; pass < 4; ++pass) {
        int shift = pass * 8;
        if (((bitOr >> shift) & 0xFF) == 0) continue;

        alignas(64) uint32_t count[256] = {};

        // Combined count pass (1 KB)
        for (size_t i = 0; i < n; ++i) {
            count[(src[i] >> shift) & 0xFF]++;
        }

        // Prefix sum
        uint32_t sum = 0;
        for (int i = 0; i < 256; ++i) {
            uint32_t c = count[i];
            count[i] = sum;
            sum += c;
        }

        if (count[0] == n) continue;

        // Block-buffered 16-byte SIMD store scatter
        std::memset(wcnt, 0, 256);
        for (size_t i = 0; i < n; ++i) {
            u32 v = src[i];
            uint8_t b = (v >> shift) & 0xFF;
            wbuf[b][wcnt[b]++] = v;
            if (wcnt[b] == 4) {
                std::memcpy(&dst[count[b]], wbuf[b], 16);
                count[b] += 4;
                wcnt[b] = 0;
            }
        }
        for (int b = 0; b < 256; ++b) {
            if (wcnt[b] > 0) {
                std::memcpy(&dst[count[b]], wbuf[b], wcnt[b] * sizeof(u32));
                count[b] += wcnt[b];
                wcnt[b] = 0;
            }
        }
        std::swap(src, dst);
    }

    if (src != data) std::memcpy(data, src, n * sizeof(u32));
}

inline void sort_univ(u32* data, size_t n, u32 bitOr = ~0u) {
    radixSort8_univ(data, n, bitOr);
}

} // namespace qi_univ

#endif // QI_SORT_UNIV_HPP
