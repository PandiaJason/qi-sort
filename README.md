<div align="center">

<br/>

<img src="https://img.shields.io/badge/qi--sort-blueviolet?style=for-the-badge&labelColor=0d1117" alt="qi-sort" height="48"/>

<h3>Quantum-Inspired Adaptive Radix Sorting Engine</h3>

<p>
A <b>header-only</b> C++17 sorting library that senses data distribution at runtime<br/>
and dispatches to the optimal radix kernel — up to <b>5.88× faster than <code>std::sort</code></b> on real data<br/>
and up to <b>18.6× faster (814 MKeys/s)</b> in parallel multi-threaded mode.
</p>

<p>

[![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg?style=flat-square)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=c%2B%2B)](include/qi_radix.hpp)
[![Python](https://img.shields.io/badge/Python-3.7%2B-3776AB?style=flat-square&logo=python&logoColor=white)](bindings/python/qi_sort.py)
[![Java](https://img.shields.io/badge/Java-JNI-ED8B00?style=flat-square&logo=openjdk&logoColor=white)](bindings/java/com/qisort/QiSort.java)
[![Header Only](https://img.shields.io/badge/header--only-yes-brightgreen?style=flat-square)](include/qi_radix.hpp)
[![Zero Dependencies](https://img.shields.io/badge/dependencies-zero-success?style=flat-square)](include/qi_radix.hpp)
[![Audit](https://img.shields.io/badge/audit-25%2F25_pass-success?style=flat-square)](benchmarks/verify_implementation.cpp)

</p>

<p>
  <a href="#what-is-qi-sort">What is QI Sort?</a> ·
  <a href="#quickstart">Quickstart</a> ·
  <a href="#real-world-benchmarks">Benchmarks</a> ·
  <a href="#head-to-head-competitors">Head-to-Head</a> ·
  <a href="#kernel-selection-ablation">Ablation Study</a> ·
  <a href="#api-reference">API</a> ·
  <a href="#how-it-works">How It Works</a> ·
  <a href="#honest-analysis">Honest Analysis</a>
</p>

</div>

<br/>

---

## What is QI Sort?

Most sorting libraries give you one static algorithm and hope it fits your data. **QI Sort does something different** — before sorting a single element, it *reads the data's distribution state* and dispatches to the optimal radix kernel for that exact input.

It does this using math derived from **quantum mechanics**: the Inverse Participation Ratio (IPR) metric used in condensed-matter physics to measure wavefunction concentration across energy states:

$$\psi_i = \sqrt{p_i}, \qquad \text{IPR} = \sum p_i^2, \qquad N_{\text{eff}} = \frac{1}{\text{IPR}}$$

Applied to byte histograms of your data, $N_{\text{eff}}$ tells us how many buckets a radix pass will *actually* touch — which directly predicts CPU L1/L2 cache pressure.

From that measurement, QI Sort dispatches to one of three built-in radix kernels:

| Kernel | Bucket width | Count array size | Memory passes | Best for |
| :--- | :---: | :---: | :---: | :--- |
| **Radix-16** | 16 bits | 65,536 (512 KB) | 2 | Low-entropy, bounded range, pre-sorted, timestamps |
| **Radix-11** | 11 bits | 2,048 (16 KB) | 3 | High-entropy data where R-16 would trash L2 cache |
| **Radix-8** | 8 bits | 256 (2 KB) | 4 | Extremely tight L1 cache memory constraints |

There is also an **O(N) short-circuit**: if sensing detects the data is pre-sorted or reverse-sorted, `qi::sort` returns immediately — up to 22× faster than standard sorting.

**Multi-Threaded Parallel Execution:** Passing `options.parallel = true` parallelizes histogram accumulation and scatter phases across all CPU cores with zero inter-thread locks, achieving up to **814 MKeys/sec** on 10 million keys.

---

## Quickstart

**C++ — drop in one header:**

```bash
cp include/qi_radix.hpp /your/project/include/
```

```cpp
#include "qi_radix.hpp"

// Single-threaded (default)
std::vector<uint32_t> data = {10543, 42, 999999, 12, 0, 8881};
qi::sort(data);

// Multi-threaded (all CPU cores)
qi::SortOptions opts;
opts.parallel = true;
qi::sort(data, opts);
```

**Python integration:**

```python
import qi_sort
qi_sort.sort(my_list)   # 2.32× faster than built-in list.sort()
```

---

## Real-World Benchmarks

> **No synthetic data.** All datasets are public, real-world physical datasets.
> Re-run: `g++ -O3 -std=c++17 -march=native benchmarks/real_data_benchmark.cpp -o bench && ./bench`

### Dataset 1 — NYC Yellow Taxi Trip Timestamps
**Source:** NYC Open Data / TLC Trip Records · 87,921 Unix epoch timestamps

| Algorithm | Time | vs `std::sort` | vs Plain Radix-16 |
| :--- | ---: | :---: | :---: |
| `std::sort` (Introsort) | 1.58 ms | baseline | 0.25× |
| `std::stable_sort` (Timsort) | 1.05 ms | 1.50× | 0.39× |
| Plain Radix-16 | 0.41 ms | 3.85× | baseline |
| **`qi::sort` [Auto: R-16]** | **0.50 ms** | **3.18× faster** | **0.82×** |

---

### Dataset 2 — English Dictionary Words (CRC-32 Hashes)
**Source:** `/usr/share/dict/words` (macOS system) · 943,904 CRC-32 hashes

| Algorithm | Time | vs `std::sort` | vs Plain Radix-16 |
| :--- | ---: | :---: | :---: |
| `std::sort` (Introsort) | 19.78 ms | baseline | 0.23× |
| `std::stable_sort` (Timsort) | 14.58 ms | 1.35× | 0.31× |
| Plain Radix-16 (Fixed) | 4.56 ms | 4.34× | baseline |
| **`qi::sort` [Auto: R-11]** | **3.36 ms** | **5.88× faster** | **1.35× FASTER** |

*QI detects high per-byte entropy and selects Radix-11 (2,048 buckets), keeping count arrays inside L1 cache to beat fixed Radix-16 by 1.35×.*

---

### Dataset 3 — Airport Elevation Data
**Source:** OurAirports Open Data · 851,316 elevation values (ft)

| Algorithm | Time | vs `std::sort` | vs Plain Radix-16 |
| :--- | ---: | :---: | :---: |
| `std::sort` (Introsort) | 9.00 ms | baseline | 0.48× |
| `std::stable_sort` (Timsort) | 13.01 ms | 0.69× | 0.33× |
| Plain Radix-16 (Fixed) | 4.33 ms | 2.07× | baseline |
| **`qi::sort` [Auto: R-16]** | **2.75 ms** | **3.27× faster** | **1.57× FASTER** |

---

### Aggregate — All Real Datasets Combined

| Algorithm | Combined Time | vs `std::sort` | vs Plain Radix-16 |
| :--- | ---: | :---: | :---: |
| `std::sort` (Introsort) | 30.37 ms | baseline | 0.31× |
| `std::stable_sort` (Timsort) | 28.64 ms | 1.06× | 0.32× |
| Plain Radix-16 (Fixed) | 9.30 ms | 3.27× | baseline |
| **`qi::sort` (Our Engine)** | **6.61 ms** | **4.59× FASTER** | **1.41× FASTER** |

**`qi::sort` is 4.59× faster than `std::sort` and 1.41× faster than fixed Radix-16 across real physical datasets.**

---

## Head-to-Head Competitor Benchmark

Tested against **pdqsort** (Pattern-Defeating QuickSort by Orson Peters, used in **Rust's standard library**) and **ska_sort** (Malte Skarupke's radix sort, widely cited as one of the fastest open-source implementations).

Compiled with `g++ -O3 -std=c++17 -march=native`, 10,000,000 keys, best of 3 runs:

### Single-Threaded Results (1 Thread)

| Dataset | `std::sort` | pdqsort | ska_sort | **`qi::sort`** | Winner |
| :--- | ---: | ---: | ---: | ---: | :--- |
| **Uniform Random 32-bit** | 228.49 ms | 220.72 ms | 178.85 ms | **42.17 ms** | **`qi::sort` (5.4×)** |
| **Nearly Sorted (95%)** | 122.73 ms | 119.98 ms | 160.74 ms | **98.31 ms** | **`qi::sort` (1.2×)** |
| **Few Unique (1000 values)** | 95.33 ms | 70.22 ms | 76.88 ms | **31.37 ms** | **`qi::sort` (3.0×)** |
| **Pipe Organ Pattern** | 572.85 ms | 237.60 ms | 157.90 ms | **99.03 ms** | **`qi::sort` (5.8×)** |
| **Random 0–65535 (16-bit)** | 138.80 ms | 113.09 ms | 79.96 ms | **43.55 ms** | **`qi::sort` (3.2×)** |

**Single-threaded scorecard: `qi::sort` wins 5/5 — undefeated across all distributions.**

### Parallel Results (Multi-Threaded, 10 Cores)

| Dataset | Best Single-Thread | **`qi::sort` Parallel** | Throughput | vs `std::sort` |
| :--- | ---: | ---: | ---: | :---: |
| **Uniform Random 32-bit** | 42.17 ms (`qi` scalar) | **12.28 ms** | **814 MKeys/s** | **18.6× FASTER** |
| **Nearly Sorted (95%)** | 98.31 ms (`qi` scalar) | **28.33 ms** | 352 MKeys/s | **4.3× FASTER** |
| **Few Unique (1000 values)** | 31.37 ms (`qi` scalar) | **14.76 ms** | 677 MKeys/s | **6.5× FASTER** |
| **Pipe Organ Pattern** | 99.03 ms (`qi` scalar) | **30.57 ms** | 327 MKeys/s | **18.7× FASTER** |
| **Random 0–65535 (16-bit)** | 43.55 ms (`qi` scalar) | **22.44 ms** | 445 MKeys/s | **6.2× FASTER** |

**Parallel scorecard: `qi::sort` wins 5/5 — undefeated across all distributions.**

---

## Kernel-Selection Ablation & Regret Analysis

To verify that `qi::sort`'s adaptive sensing mechanism actually predicts the optimal radix kernel rather than guessing, we ran a kernel selection ablation experiment calculating selection regret:

$$\text{Regret} = \frac{T_{\text{QI}} - T_{\text{oracle}}}{T_{\text{oracle}}} \times 100\%$$

| Metric | Result |
| :--- | :---: |
| **Oracle Selection Match Rate** | **7 / 7 (100.0%)** |
| **Mean Selection Regret** | **1.31%** (including full sensing overhead) |
| **Verification Suite Audit** | **25 / 25 PASSED** |

---

**Implementation audit:** [`benchmarks/verify_implementation.cpp`](benchmarks/verify_implementation.cpp) independently checks 25 claims — `psi = sqrt(p)`, IPR formula, N_eff, cost model dispatch, short-circuit speedup, correctness, and timing honesty. **25/25 pass.**

---

## Installation

### C++ — Header Only

```bash
cp include/qi_radix.hpp /your/project/include/
```

```cpp
#include "qi_radix.hpp"
```

### CMake

```cmake
add_subdirectory(path/to/qi-sort)
target_link_libraries(your_target PRIVATE qi_radix)
```

### Build Shared Library (Python / Java / C bindings)

```bash
# macOS
g++ -O3 -shared -fPIC -std=c++17 -march=native src/qi_c_api.cpp -o libqisort.dylib

# Linux
g++ -O3 -shared -fPIC -std=c++17 -march=native src/qi_c_api.cpp -o libqisort.so
```

---

## Usage

### C++

```cpp
#include "qi_radix.hpp"

// Sort a vector
std::vector<uint32_t> data = {10543, 42, 999999, 12, 0, 8881};
qi::sort(data);

// Sort a raw array
uint32_t arr[] = {500, 20, 100, 5, 200, 10};
qi::sort(arr, 6);

// Iterator range
qi::sort(data.begin(), data.end());

// Verbose mode — prints live telemetry
qi::SortOptions opts;
opts.verbose = true;
qi::sort(data, opts);
// [QI-Radix] N=6 | Selected=RADIX-11 | Entropy=0.1011 | EffectiveStates=2.35
```

### Python

```python
import qi_sort

# Sort a list — 2.32× faster than list.sort()
data = [10543, 42, 999999, 12, 0, 8881]
qi_sort.sort(data)
# → [0, 12, 42, 8881, 10543, 999999]

# Zero-copy NumPy buffer sort
import numpy as np
arr = np.random.randint(0, 2**32, size=1_000_000, dtype=np.uint32)
qi_sort.sort(arr)

# Non-destructive distribution inspection
stats = qi_sort.analyze(data)
# → {'entropy': 0.1582, 'ipr': 0.5714, 'effective_states': 2.97, 'duplicate_ratio': 0.0}
```

### Java (JNI)

```java
import com.qisort.QiSort;

int[] data = {10543, 42, 999999, 12, 0, 8881};
QiSort.sort(data);
```

### C / Rust / Go / Node.js (C-ABI)

```c
#include "qi_c_api.h"

uint32_t data[] = {10543, 42, 999999, 12, 0, 8881};
qi_sort_u32(data, 6);
```

---

## API Reference

### `qi::sort`

```cpp
void qi::sort(std::vector<uint32_t>& data, qi::SortOptions opts = {});
void qi::sort(uint32_t* data, size_t n, qi::SortOptions opts = {});

template <typename RandomIt>
void qi::sort(RandomIt begin, RandomIt end, qi::SortOptions opts = {});
```

### `qi::analyze` — non-destructive inspection

```cpp
qi::State qi::analyze(const std::vector<uint32_t>& data, size_t sampleSize = 8192);
qi::State qi::analyze(const uint32_t* data, size_t n, size_t sampleSize = 8192);
```

**`qi::State` fields:**

| Field | Type | Description |
| :--- | :--- | :--- |
| `averageEntropy` | `double` | Shannon entropy across byte lanes |
| `amplitudeConcentration` | `double` | Inverse Participation Ratio (IPR) = $\sum p_i^2$ |
| `effectiveStates` | `double` | Effective occupied bucket count $N_{\text{eff}} = 1/\text{IPR}$ |
| `duplicateRatio` | `double` | Fraction of duplicate values `[0, 1]` |
| `orderedness` | `double` | Degree of pre-sorted order `[0, 1]` |
| `analysisTimeMs` | `double` | Time spent in sensing phase (ms) |
| `recommendedRadix` | `qi::Radix` | `R8`, `R11`, or `R16` |

### `qi::SortOptions`

| Field | Default | Description |
| :--- | :--- | :--- |
| `sampleSize` | `8192` | Elements sampled for distribution sensing |
| `allowShortcuts` | `true` | Enable $O(N)$ early-exit for sorted/reverse input |
| `verbose` | `false` | Print live telemetry to `stdout` |
| `parallel` | `false` | Enable multi-threaded parallel radix sort engine |
| `numThreads` | `0` | Threads for parallel mode (0 = auto-detect hardware cores) |

---

## CLI Utility — `qsort-db`

A production binary for benchmarking real binary disk files.

```bash
# Build
g++ -O3 -std=c++17 -march=native src/qsort_cli.cpp -o qsort-db

# Generate a 95 MB binary test file
./qsort-db --generate 25000000 data.bin

# Benchmark all three algorithms on the file
./qsort-db --benchmark data.bin
```

---

## Where to Use qi-sort

| Domain | Use Case |
| :--- | :--- |
| **Columnar Databases** | `ORDER BY` / `GROUP BY` on integer surrogate keys (DuckDB, ClickHouse, Arrow) |
| **Search Engines** | Posting list and document ID sorting in inverted index pipelines |
| **3D Graphics / Games** | Morton Z-order BVH construction for real-time ray tracing |
| **Network Telemetry** | High-frequency IP flow ID and packet timestamp ordering |
| **Scientific Computing** | Radix histogram sorts in particle simulation pipelines |
| **High-Frequency Trading** | Nanosecond-timestamped order book event sorting |

---

## How It Works

**Step 1 — Sample & Sense.**
On a sample of `sampleSize` elements, qi-sort builds per-byte histograms and computes probability amplitudes and concentration using quantum-physics-derived metrics:

$$\psi_{b,i} = \sqrt{p_{b,i}}, \qquad \text{IPR}_b = \sum_{i=0}^{255} p_{b,i}^2, \qquad N_{\text{eff},b} = \frac{1}{\text{IPR}_b}$$

The IPR (Inverse Participation Ratio) is a real condensed-matter physics metric that measures how localized a probability distribution is across basis states.

**Step 2 — Predict Cache Pressure.**
For Radix-16, the estimated active bucket footprint is:

$$B_{16} = \min(65536,\ N_{\text{eff},0} \times N_{\text{eff},1})$$

When $B_{16}$ exceeds L2 cache capacity, the cost penalty shifts the optimal choice to Radix-11.
A cardinality guard (`duplicateRatio > 0.90`) bypasses the cost model for high-duplicate data and forces R-16 directly.

**Step 3 — Execute.**
The selected kernel runs with heap-allocated count arrays, 4-way loop unrolling, and software prefetch (`__builtin_prefetch`). Fully sorted or reverse-sorted inputs short-circuit in $O(N)$ — achieving **22× speedup** vs full radix passes on pre-ordered data.

> The sensing overhead is **4–8% of total sort time** (verified in audit). It is charged inside the `qi::sort` timing, not excluded.

---

## Repository Structure

```
qi-sort/
├── include/
│   ├── qi_radix.hpp                      # ← start here: C++17 header-only core
│   └── qi_c_api.h                        # C-ABI interface (Python / Java / Rust / Go)
├── src/
│   ├── qi_c_api.cpp                      # C-ABI implementation
│   ├── qi_jni.cpp                        # Java JNI bridge
│   └── qsort_cli.cpp                     # qsort-db CLI utility
├── bindings/
│   ├── python/
│   │   ├── qi_sort.py                    # Python ctypes / NumPy integration
│   │   └── test_python.py                # Python benchmark
│   └── java/
│       └── com/qisort/QiSort.java        # Java JNI wrapper
├── benchmarks/
│   ├── kernel_selection_ablation.cpp     # Oracle vs QI regret analysis
│   ├── head_to_head.cpp                  # vs pdqsort (Rust std) & ska_sort (Skarupke)
│   ├── parallel_benchmark.cpp            # Multi-threaded parallel scaling benchmark
│   ├── real_data_benchmark.cpp           # NYC taxi + dictionary + airports
│   ├── real_world_database_benchmark.cpp # 40M row columnar DB simulation
│   ├── plain_radix_vs_qi.cpp             # Algorithmic fairness test
│   ├── algo_fairness_test.cpp            # Pure C++ algorithm comparison
│   ├── cost_model_comparison.cpp         # QI vs entropy threshold vs always-R16
│   └── verify_implementation.cpp         # 25-check implementation audit
├── examples/
│   ├── basic_usage.cpp
│   ├── analytical_inspection.cpp
│   ├── database_column_sort.cpp
│   ├── spatial_morton_sort.cpp
│   └── c_api_usage.c
├── CMakeLists.txt
├── LICENSE                               # GNU General Public License v2.0
└── README.md
```

---

## License

Licensed under the **GNU General Public License v2.0** — the same license as the Linux Kernel. See [LICENSE](LICENSE).

---

## Citation

If you use qi-sort in academic work:

```bibtex
@software{pandia2026qisort,
  title   = {QI-Sort: Quantum-Inspired Adaptive Radix Sorting Engine},
  author  = {Pandia, Jason},
  year    = {2026},
  url     = {https://github.com/PandiaJason/qi-sort},
  license = {GPL-2.0}
}
```

---

## Contributing

Issues and pull requests are welcome.
When submitting a performance claim, please include output from `benchmarks/verify_implementation.cpp` and `benchmarks/real_data_benchmark.cpp`.

<div align="center">
<br/>
<sub>Built with C++17 · Tested on macOS 26.5 / Linux · GPL-2.0</sub>
</div>
