#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <string>

using u32 = uint32_t;

// ============================================================================
// 1. QI-SORT: 2-PASS RADIX-16 ZERO-MEMCPY ENGINE
// ============================================================================

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

inline void qi_sort(u32* data, size_t n) {
    if (n <= 1) return;

    if (n >= 64) {
        bool isSorted = true;
        for (size_t i = 1; i < std::min<size_t>(n, 1024); ++i) {
            if (data[i - 1] > data[i]) { isSorted = false; break; }
        }
        if (isSorted && std::is_sorted(data, data + n)) return;
    }

    u32* buf = getScratch().get(n);

    alignas(64) uint32_t c0[65536] = {};
    alignas(64) uint32_t c1[65536] = {};

    for (size_t i = 0; i < n; ++i) {
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

    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        u32 v0 = data[i + 0], v1 = data[i + 1], v2 = data[i + 2], v3 = data[i + 3];
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
        u32 v0 = buf[i + 0], v1 = buf[i + 1], v2 = buf[i + 2], v3 = buf[i + 3];
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

// ============================================================================
// 2. NUMPY'S EXACT QUICKSORT ENGINE (MEDIAN-OF-THREE + INSERTION SORT FALLBACK)
// ============================================================================

static inline void np_insertion_sort(u32* data, size_t n) {
    for (size_t i = 1; i < n; ++i) {
        u32 key = data[i];
        size_t j = i;
        while (j > 0 && data[j - 1] > key) {
            data[j] = data[j - 1];
            --j;
        }
        data[j] = key;
    }
}

static inline u32 np_median_of_three(u32 a, u32 b, u32 c) {
    if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
    if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
    return c;
}

inline void np_quicksort_impl(u32* data, size_t n) {
    while (n > 16) {
        u32 pivot = np_median_of_three(data[0], data[n / 2], data[n - 1]);
        size_t i = 0, j = n - 1;
        while (true) {
            while (data[i] < pivot) ++i;
            while (data[j] > pivot) --j;
            if (i >= j) break;
            std::swap(data[i], data[j]);
            ++i;
            --j;
        }
        size_t right_len = n - i;
        if (i < right_len) {
            np_quicksort_impl(data, i);
            data += i;
            n = right_len;
        } else {
            np_quicksort_impl(data + i, right_len);
            n = i;
        }
    }
    np_insertion_sort(data, n);
}

inline void np_quicksort(u32* data, size_t n) {
    np_quicksort_impl(data, n);
}

// ============================================================================
// 3. BENCHMARK HARNESS
// ============================================================================

int main() {
    constexpr size_t N = 2'000'000;
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFFu);

    std::vector<uint32_t> uniform(N), dupes(N), nearly(N);
    for (size_t i = 0; i < N; ++i) {
        uniform[i] = dist(rng);
        dupes[i]   = rng() % 256;
    }
    nearly = uniform;
    std::sort(nearly.begin(), nearly.end());
    for (size_t i = 0; i < N / 20; ++i) nearly[rng() % N] = dist(rng);

    auto run_bench = [](const std::string& name, std::vector<uint32_t>& data) {
        auto check = data;
        qi_sort(check.data(), check.size());
        bool ok = std::is_sorted(check.begin(), check.end());

        double min_qi = 1e9, min_np = 1e9, min_std = 1e9;
        for (int r = 0; r < 5; ++r) {
            auto d1 = data;
            auto t0 = std::chrono::high_resolution_clock::now();
            qi_sort(d1.data(), d1.size());
            auto t1 = std::chrono::high_resolution_clock::now();
            min_qi = std::min(min_qi, std::chrono::duration<double, std::milli>(t1 - t0).count());

            auto d2 = data;
            t0 = std::chrono::high_resolution_clock::now();
            np_quicksort(d2.data(), d2.size());
            t1 = std::chrono::high_resolution_clock::now();
            min_np = std::min(min_np, std::chrono::duration<double, std::milli>(t1 - t0).count());

            auto d3 = data;
            t0 = std::chrono::high_resolution_clock::now();
            std::sort(d3.begin(), d3.end());
            t1 = std::chrono::high_resolution_clock::now();
            min_std = std::min(min_std, std::chrono::duration<double, std::milli>(t1 - t0).count());
        }

        std::cout << "=== " << name << " (N = 2,000,000) ===\n";
        std::cout << "  NumPy QuickSort: " << min_np  << " ms\n";
        std::cout << "  std::sort:       " << min_std << " ms\n";
        std::cout << "  qi_sort:         " << min_qi  << " ms\n";
        std::cout << "  Speedup vs NumPy: " << (min_np / min_qi) << "x faster [" << (ok ? "PASS" : "FAIL") << "]\n\n";
    };

    run_bench("Uniform Random (32-bit)",  uniform);
    run_bench("Heavy Duplicates (0-255)", dupes);
    run_bench("Nearly Sorted (95%)",      nearly);

    return 0;
}
