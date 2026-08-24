#ifndef QI_SORT_UNIV_HPP
#define QI_SORT_UNIV_HPP

/**
 * qi-sort-univ: OpenMP Multi-Threaded 2-Pass Radix-16 Zero-Memcpy Engine
 * ======================================================================
 * Engineered for Linux cloud hypervisors (Google Colab / AWS / GCP / Docker).
 *
 * KEY HIGHLIGHTS:
 * 1. OpenMP Parallelization: Distributes Pass 0 and Pass 1 across all vCPU cores.
 * 2. 0 Memcpy: Pass 0 writes data -> buf; Pass 1 writes buf -> data.
 * 3. 0 TLS Overhead: Thread-local static histograms.
 */

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <algorithm>
#include <array>

#if defined(_OPENMP)
  #include <omp.h>
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

// ── FAST 2-PASS RADIX-16 ZERO-MEMCPY ENGINE ──
inline void radixSort16_univ(u32* data, size_t n) {
    if (n <= 1) return;
    u32* buf = getScratch().get(n);

#if defined(_OPENMP)
    if (n >= 100000) {
        int nth = omp_get_max_threads();
        if (nth > 1) {
            std::vector<std::array<uint32_t, 65536>> c0_t(nth);
            std::vector<std::array<uint32_t, 65536>> c1_t(nth);

            #pragma omp parallel
            {
                int tid = omp_get_thread_num();
                c0_t[tid].fill(0);
                c1_t[tid].fill(0);
                #pragma omp for schedule(static)
                for (size_t i = 0; i < n; ++i) {
                    u32 v = data[i];
                    c0_t[tid][v & 0xFFFFu]++;
                    c1_t[tid][v >> 16]++;
                }
            }

            bool skipPass1 = true;
            for (int t = 0; t < nth; ++t) {
                if (c1_t[t][0] != c1_t[t][0]) { skipPass1 = false; break; }
            }

            std::vector<std::array<uint32_t, 65536>> off0(nth);
            std::vector<std::array<uint32_t, 65536>> off1(nth);
            uint32_t s0 = 0, s1 = 0;
            for (int k = 0; k < 65536; ++k) {
                for (int t = 0; t < nth; ++t) {
                    off0[t][k] = s0; s0 += c0_t[t][k];
                    off1[t][k] = s1; s1 += c1_t[t][k];
                }
            }

            #pragma omp parallel
            {
                int tid = omp_get_thread_num();
                auto l_off0 = off0[tid];
                #pragma omp for schedule(static)
                for (size_t i = 0; i < n; ++i) {
                    u32 v = data[i];
                    buf[l_off0[v & 0xFFFFu]++] = v;
                }
            }

            #pragma omp parallel
            {
                int tid = omp_get_thread_num();
                auto l_off1 = off1[tid];
                #pragma omp for schedule(static)
                for (size_t i = 0; i < n; ++i) {
                    u32 v = buf[i];
                    data[l_off1[v >> 16]++] = v;
                }
            }
            return;
        }
    }
#endif

    // Single-threaded fallback
    alignas(64) uint32_t c0[65536] = {};
    alignas(64) uint32_t c1[65536] = {};

    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        u32 v0 = data[i+0], v1 = data[i+1], v2 = data[i+2], v3 = data[i+3];
        u32 v4 = data[i+4], v5 = data[i+5], v6 = data[i+6], v7 = data[i+7];
        c0[v0 & 0xFFFFu]++; c1[v0 >> 16]++;
        c0[v1 & 0xFFFFu]++; c1[v1 >> 16]++;
        c0[v2 & 0xFFFFu]++; c1[v2 >> 16]++;
        c0[v3 & 0xFFFFu]++; c1[v3 >> 16]++;
        c0[v4 & 0xFFFFu]++; c1[v4 >> 16]++;
        c0[v5 & 0xFFFFu]++; c1[v5 >> 16]++;
        c0[v6 & 0xFFFFu]++; c1[v6 >> 16]++;
        c0[v7 & 0xFFFFu]++; c1[v7 >> 16]++;
    }
    for (; i < n; ++i) {
        u32 v = data[i];
        c0[v & 0xFFFFu]++;
        c1[v >> 16]++;
    }

    bool skipPass1 = (c1[0] == n);

    uint32_t s0 = 0, s1 = 0;
    for (int k = 0; k < 65536; ++k) {
        uint32_t t0 = c0[k]; c0[k] = s0; s0 += t0;
        if (!skipPass1) { uint32_t t1 = c1[k]; c1[k] = s1; s1 += t1; }
    }

    i = 0;
    for (; i + 4 <= n; i += 4) {
        u32 v0 = data[i+0], v1 = data[i+1], v2 = data[i+2], v3 = data[i+3];
        buf[c0[v0 & 0xFFFFu]++] = v0;
        buf[c0[v1 & 0xFFFFu]++] = v1;
        buf[c0[v2 & 0xFFFFu]++] = v2;
        buf[c0[v3 & 0xFFFFu]++] = v3;
    }
    for (; i < n; ++i) {
        u32 v = data[i];
        buf[c0[v & 0xFFFFu]++] = v;
    }

    if (skipPass1) {
        std::memcpy(data, buf, n * sizeof(u32));
        return;
    }

    i = 0;
    for (; i + 4 <= n; i += 4) {
        u32 v0 = buf[i+0], v1 = buf[i+1], v2 = buf[i+2], v3 = buf[i+3];
        data[c1[v0 >> 16]++] = v0;
        data[c1[v1 >> 16]++] = v1;
        data[c1[v2 >> 16]++] = v2;
        data[c1[v3 >> 16]++] = v3;
    }
    for (; i < n; ++i) {
        u32 v = buf[i];
        data[c1[v >> 16]++] = v;
    }
}

inline void sort_univ(u32* data, size_t n) {
    radixSort16_univ(data, n);
}

} // namespace qi_univ

#endif // QI_SORT_UNIV_HPP
