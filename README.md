# `qi::apex`

Hardware-aware adaptive radix sort (`qi::apex`) is a high-performance sorting engine designed for modern CPU microarchitectures. It combines ultra-lightweight distribution sensing with strict L1-data-cache-bounded radix passes and vectorized counting sort, achieving linear $O(n)$ time on structured patterns and outperforming comparison-based algorithms (`std::sort`, `pdqsort`) by **6×–20×** on numeric keys. All code is available for free under the GPL-2.0 license.

| Best | Average | Worst | Memory | Stable | Deterministic |
|:---:|:---:|:---:|:---:|:---:|:---:|
| $n$ | $n$ | $n$ | $n$ | No | Yes |

---

## Models & Usage

The library provides two specialized header-only C++17 engines:

### 1. `qi::apex` — Hardware-Aware L1-Bound Engine (`#include "qi_apex.hpp"`)

Designed for peak throughput on numeric arrays by strictly confining histogram memory to the CPU L1-Data cache:

```cpp
#include "qi_apex.hpp"
#include <vector>

int main() {
    // 1. Unsigned 32-bit integers (36.5 ms for 10,000,000 keys)
    std::vector<uint32_t> data = {42, 10, 100, 5, 9999, 12};
    qi::apex::sort(data.data(), data.size());

    // 2. Signed integers & IEEE 754 floating-point numbers
    std::vector<float> floats = {-3.14f, 0.0f, 99.8f, -0.001f};
    qi::apex::sort(floats.data(), floats.size());

    // 3. 64-bit integers (uint64_t, int64_t, double)
    std::vector<uint64_t> u64_data = {1000000000000ULL, 500ULL, 42ULL};
    qi::apex::sort(u64_data.data(), u64_data.size());

    // 4. Database Column ORDER BY (Key-Payload Pairs)
    std::vector<uint32_t> keys    = {40, 10, 30};
    std::vector<uint64_t> row_ids = {101, 102, 103};
    qi::apex::sort_pairs(keys.data(), row_ids.data(), keys.size());

    // 5. Multi-Threaded Parallel Execution (17.5 ms for 10,000,000 keys)
    qi::apex::parallel_sort(data.data(), data.size());
}
```

### 2. `qi::sort` — Autonomous Adaptive Sensing Router (`#include "qi_radix.hpp"`)

Runs a ~50ns pure-integer bitwise probe to classify entropy and automatically dispatches to Counting Sort, Radix-8, Radix-11, or Radix-16:

```cpp
#include "qi_radix.hpp"
#include <vector>

int main() {
    std::vector<uint32_t> data = {500, 12, 99, 1024, 0, 42};
    
    // Auto-detects distribution and routes to optimal kernel
    qi::sort(data);
}
```

---

## Benchmark

A comparison of `qi::apex`, Orson Peters' `pdqsort`, Malte Skarupke's `ska_sort`, and GCC/Clang's `std::sort` with various input distributions:

<p align="center">
  <img src="docs/benchmark_chart.svg" alt="qi::apex Benchmark Chart" width="100%"/>
</p>

*Compiled with `clang++ -std=c++17 -O3 -m64 -march=native` on Apple Silicon M1 Pro. All timings are the best of 3 runs on $N = 10,000,000$ keys (40 MB RAM).*

### Single-Threaded (1T) Execution ($N = 10,000,000$ keys)

| Distribution | `std::sort` | `pdqsort` | `ska_sort` | `qi::sort` | **`qi::apex` (1T)** | Speedup vs `std::sort` | Speedup vs `pdqsort` |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Uniform Random 32-bit** | 230.06 ms | 220.88 ms | 84.97 ms | 37.68 ms | **`36.59 ms`** | **6.3×** | **6.0×** |
| **Byte Duplicates (0–255)** | 81.33 ms | 58.35 ms | 147.76 ms | **`4.09 ms`** | **`4.13 ms`** | **19.7×** | **14.1×** |
| **Periodic Sawtooth (1K)** | 106.49 ms | 84.38 ms | 145.16 ms | **`4.07 ms`** | **`4.03 ms`** | **26.4×** | **20.9×** |
| **16-bit Range (0–65,535)** | 135.34 ms | 110.47 ms | 115.44 ms | 40.19 ms | **`21.76 ms`** | **6.2×** | **5.1×** |
| **Already Sorted Monotonic** | 10.46 ms | 7.66 ms | 146.58 ms | **`4.40 ms`** | **`4.41 ms`** | **2.4×** | **1.7×** |
| **Reverse Sorted Monotonic** | 17.59 ms | 13.63 ms | 151.34 ms | 6.39 ms | **`6.33 ms`** | **2.8×** | **2.2×** |
| **Nearly Sorted (~98%)** | 191.40 ms | 170.06 ms | 145.57 ms | 192.99 ms | **`64.33 ms`** | **3.0×** | **2.6×** |
| **Pipe Organ (Mirrored Ramp)**| 583.26 ms | 248.56 ms | 143.59 ms | 135.83 ms | **`134.05 ms`**| **4.4×** | **1.9×** |

### Multi-Threaded (MT) Scaling ($N = 10,000,000$ keys)

| Workload | `std::sort` (1T) | `pdqsort` (1T) | `qi::apex` (1T) | **`qi::apex` (Parallel MT)** | Parallel Throughput |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **10M Uniform Random 32-bit** | 230.06 ms | 220.88 ms | 36.59 ms | **`17.59 ms`** | **568.5 MKeys/s** |
| **10M 16-bit Domain (0–65K)** | 135.34 ms | 110.47 ms | 21.76 ms | **`13.89 ms`** | **720.0 MKeys/s** |
| **10M Low-Cardinality (4 vals)**| 50.73 ms | 16.65 ms | 12.30 ms | **`9.36 ms`** | **1,068.3 MKeys/s** |

*(Note: On systems equipped with AVX-512 vector register files such as Intel Xeon Platinum, in-register SIMD sorters like Google VQSort hold the throughput lead on uniform random noise; `qi::apex` achieves high throughput universally across Apple Silicon, ARM, AMD, and Intel architectures without requiring specialized vector instruction sets.)*

---

## Visualization

A visualization of `qi::apex` sorting a ~140 element array. The animation shows the transition from randomized input entropy through 50ns sensing and 3-pass L1-bound radix partitioning into a fully sorted array:

<p align="center">
  <img src="docs/sorting_visualization.gif" alt="qi::apex Array Sorting Simulation" width="100%"/>
</p>

---

## The Best Case

`qi::apex` is designed to run in linear $O(n)$ time for common structural patterns. Linear time is achieved for inputs that are pre-sorted, reverse-sorted, contain low cardinality / duplicate keys, or fall within narrow domains.

There are three separate mechanisms at play to achieve this:

1. **Monotonic Short-Circuits:** Before partitioning, `qi::apex` runs a strided sample check across 64 elements. If the sample indicates ascending order, a linear verification pass confirms sortedness and returns immediately in $O(n)$ time ($4.41\text{ ms}$ for 10M keys). If descending, it reverses in-place in $6.33\text{ ms}$. Random data is rejected in 2–3 CPU cycles.
2. **Vectorized Counting Sort (Values $\le 4,095$):** If the bitwise OR probe reveals values within 12 bits, `qi::apex` bypasses radix scatter and runs a single-pass counting sort entirely within L1 cache, running at **2,400+ MKeys/s** ($4.13\text{ ms}$ for 10M keys).
3. **Radix-16 for Heavy Duplicates:** When duplicate density is high (few unique buckets occupied), `qi::apex` automatically switches to a 2-pass Radix-16 kernel ($65,536$ bins), sorting the data in two fast passes.

---

## The Average Case

On uniform random data where no narrow patterns are detected, `qi::apex` executes an **L1-cache-bounded Radix-11** algorithm ($2,048$ bins per pass, 3 passes total for 32-bit keys).

`qi::apex` achieves its speedup over traditional radix sorts through three microarchitectural optimizations:

### 1. Strict 20 KB L1-Data Cache Bounding
Traditional 16-bit radix sorts allocate 256 KB histogram tables, exceeding the 32 KB/48 KB L1-Data caches of modern processors and causing severe cache thrashing. `qi::apex` bounds its three combined histogram tables to **exactly 20 KB** ($8\text{ KB} + 8\text{ KB} + 4\text{ KB}$), guaranteeing 100% L1-Data cache residency.

### 2. 8-Way Instruction-Level Parallelism (ILP)
Counting and scatter loops are unrolled 8 ways across independent accumulators. This eliminates Read-After-Write (RAW) pipeline hazards and saturates multiple superscalar execution ports simultaneously:

```cpp
// 8-Way Unrolled Counting Loop
for (; i + 7 < n; i += 8) {
    u32 v0 = data[i],   v1 = data[i+1], v2 = data[i+2], v3 = data[i+3];
    u32 v4 = data[i+4], v5 = data[i+5], v6 = data[i+6], v7 = data[i+7];

    c0[v0 & 0x7FF]++; c1[(v0 >> 11) & 0x7FF]++; c2[v0 >> 22]++;
    c0[v1 & 0x7FF]++; c1[(v1 >> 11) & 0x7FF]++; c2[v1 >> 22]++;
    c0[v2 & 0x7FF]++; c1[(v2 >> 11) & 0x7FF]++; c2[v2 >> 22]++;
    c0[v3 & 0x7FF]++; c1[(v3 >> 11) & 0x7FF]++; c2[v3 >> 22]++;
    c0[v4 & 0x7FF]++; c1[(v4 >> 11) & 0x7FF]++; c2[v4 >> 22]++;
    c0[v5 & 0x7FF]++; c1[(v5 >> 11) & 0x7FF]++; c2[v5 >> 22]++;
    c0[v6 & 0x7FF]++; c1[(v6 >> 11) & 0x7FF]++; c2[v6 >> 22]++;
    c0[v7 & 0x7FF]++; c1[(v7 >> 11) & 0x7FF]++; c2[v7 >> 22]++;
}
```

### 3. Pipelined Software Prefetching ($PF=48$)
Scatter passes issue `__builtin_prefetch` instructions 48 elements ahead of the active write stream. This hides DRAM write-allocate latency and maximizes memory bus throughput.

---

## The Worst Case

Unlike comparison-based quicksort (which can degrade to $O(n^2)$ on adversarial pivot choices), `qi::apex` is an LSD radix sort with a strictly deterministic runtime:

$$T(n) = O(k \cdot n)$$

Where $k$ is the number of digit passes ($k = 3$ for 32-bit keys, $k = 4$ for 64-bit keys). The worst-case runtime on completely random, adversarial data remains strictly linear with respect to the number of bytes sorted ($36.59\text{ ms}$ for 10M keys).

---

## Language Bindings

### Python (`pip install qi-sort`)
Zero-copy NumPy array sorting via dynamic C ABI loading:

```python
import numpy as np
import qi_sort

arr = np.random.randint(0, 1000000, size=10_000_000, dtype=np.uint32)
qi_sort.sort(arr)
```

### Go Module
```go
import "github.com/PandiaJason/qi-sort/bindings/go/qisort"

data := []uint32{42, 10, 100, 5, 9999}
qisort.SortU32(data)
```

---

## License & Attribution

Distributed for free under the **GPL-2.0 License**.  
Developed by **Jason Pandian** (Sole Author, 2026).

```bibtex
@software{pandia2026qiapex,
  title   = {qi::apex & qi::sort: Hardware-Aware Adaptive Radix Sorting Engine},
  author  = {Pandian, Jason},
  year    = {2026},
  url     = {https://github.com/PandiaJason/qi-sort},
  license = {GPL-2.0}
}
```
