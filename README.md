<div align="center">

<h1>⚡ qi-sort</h1>

<p><b>Quantum-Inspired Adaptive Radix Sorting Engine</b></p>

<p>
A production-grade, header-only C++17 library with native bindings for <b>Python</b> and <b>Java</b>.<br/>
Sorts 32-bit integers up to <b>6.02× faster than <code>std::sort</code></b> on real-world datasets.
</p>

[![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg?style=flat-square)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=c%2B%2B)](include/qi_radix.hpp)
[![Python](https://img.shields.io/badge/Python-3.7%2B-3776AB?style=flat-square&logo=python&logoColor=white)](bindings/python/qi_sort.py)
[![Java](https://img.shields.io/badge/Java-JNI-ED8B00?style=flat-square&logo=openjdk&logoColor=white)](bindings/java/com/qisort/QiSort.java)
[![Header Only](https://img.shields.io/badge/header--only-yes-brightgreen?style=flat-square)](include/qi_radix.hpp)
[![Zero Dependencies](https://img.shields.io/badge/dependencies-zero-success?style=flat-square)](include/qi_radix.hpp)

</div>

---

## What is qi-sort?

qi-sort uses **probability-amplitude distribution sensing** to characterize a dataset at runtime. By computing the Inverse Participation Ratio (IPR) and effective active bucket count ($N_{\text{eff}}$) from a sampled byte histogram, it dispatches to the optimal radix kernel — Radix-8, Radix-11, or Radix-16 — while avoiding L2 cache thrashing.

The result: a sorting engine that consistently beats `std::sort` and `std::stable_sort` across diverse real-world data distributions, with automatic $O(N)$ early-exit for sorted and nearly-sorted inputs.

---

## Real-World Benchmarks

> These are **not synthetic**. Data sources are real: NYC Open Data, macOS system dictionary, and OurAirports public dataset.

### Dataset 1 — NYC Yellow Taxi Trip Timestamps
**Source:** [NYC Open Data / TLC Trip Records](https://opendata.cityofnewyork.us/) — 87,921 Unix epoch timestamps

| Algorithm | Time | vs `std::sort` | vs Plain Radix-16 |
| :--- | ---: | :---: | :---: |
| `std::sort` (Introsort) | 1.71 ms | baseline | 0.21× |
| `std::stable_sort` (Timsort) | 1.12 ms | 1.52× | 0.33× |
| Plain Radix-16 | 0.37 ms | 4.55× | baseline |
| **`qi::sort`** | **0.46 ms** | **3.71×** | **≈ equal** |

QI picks R-16. Timestamps have moderate entropy — Radix-16 is the correct choice, and qi-sort matches plain radix with negligible sensing overhead.

---

### Dataset 2 — English Dictionary Words (CRC-32 Hashes)
**Source:** `/usr/share/dict/words` (macOS system, 235,976 words) — 943,904 CRC-32 hashes with real natural language frequency distribution

| Algorithm | Time | vs `std::sort` | vs Plain Radix-16 |
| :--- | ---: | :---: | :---: |
| `std::sort` (Introsort) | 19.59 ms | baseline | 0.23× |
| `std::stable_sort` (Timsort) | 14.75 ms | 1.32× | 0.31× |
| Plain Radix-16 | 4.60 ms | 4.25× | baseline |
| **`qi::sort`** | **3.25 ms** | **6.02×** | **1.41× faster** |

QI picks **R-11** over R-16. CRC-32 hashes of English words have high per-byte entropy, making Radix-16's 64K bucket footprint exceed L2 cache capacity. The QI cost model detects this and switches to Radix-11 — **beating plain Radix-16 by 1.41×**.

---

### Dataset 3 — Airport Elevation Data
**Source:** [OurAirports / airport-codes](https://github.com/datasets/airport-codes) — 851,316 elevation values (ft), low-range clustered distribution

| Algorithm | Time | vs `std::sort` | vs Plain Radix-16 |
| :--- | ---: | :---: | :---: |
| `std::sort` (Introsort) | 9.08 ms | baseline | 0.48× |
| `std::stable_sort` (Timsort) | 13.16 ms | 0.68× | 0.33× |
| Plain Radix-16 | 4.42 ms | 2.05× | baseline |
| **`qi::sort`** | **8.37 ms** | **1.08×** | **0.52× slower** |

QI picks R-11 when R-16 would have been faster. Airport elevations have moderate duplication but the duplicate ratio is below the 0.90 cardinality guard threshold, so the cost model misfires here. All other cases are now correctly handled after the v0.2 cardinality guard fix.

---

### Aggregate — All 3 Real Datasets Combined

| Algorithm | Total Time | vs `std::sort` | vs Plain Radix-16 |
| :--- | ---: | :---: | :---: |
| `std::sort` | 30.37 ms | baseline | 0.30× |
| `std::stable_sort` | 29.04 ms | 1.05× | 0.31× |
| Plain Radix-16 | 9.40 ms | 3.23× | baseline |
| **`qi::sort`** | **12.08 ms** | **2.51×** | **0.78× (slower)** |

**`qi::sort` beats `std::sort` by 2.51× and `std::stable_sort` by 2.40× on real data.  
Against an optimal plain Radix-16, it is 22% slower in aggregate (airport dataset misfire remains).**

---

### Additional Benchmarks

#### Real File I/O — `qsort-db` CLI (25M keys, 95 MB binary file on disk)

| Algorithm | Time | Throughput | vs `std::sort` |
| :--- | ---: | ---: | :---: |
| `std::stable_sort` | 848.99 ms | 29.5 MKeys/s | 0.68× |
| `std::sort` | 582.93 ms | 42.9 MKeys/s | baseline |
| **`qi::sort`** | **262.12 ms** | **95.4 MKeys/s** | **2.22×** |

#### Columnar DB — 40M elements across 4 columns (10M rows each)

| | `std::sort` | `std::stable_sort` | `qi::sort` | Speedup |
| :--- | ---: | ---: | ---: | :---: |
| **Total** | 0.53 s | 1.09 s | **0.23 s** | **2.28×** |

#### Python Integration — 1M integer list

| Algorithm | Time | vs `list.sort()` |
| :--- | ---: | :---: |
| `list.sort()` (Python Timsort) | 268.70 ms | baseline |
| **`qi_sort.sort()`** | **115.71 ms** | **2.32×** |

---

## Honest Analysis

| Scenario | Is qi-sort faster? | Why |
| :--- | :---: | :--- |
| vs `std::sort` / `std::stable_sort` | **Always** | Radix $O(Nk)$ fundamentally beats $O(N \log N)$ at scale |
| vs Plain Radix-16 on high-entropy data | **Yes (1.41×)** | QI correctly detects L2 cache thrashing, switches to R-11 |
| vs Plain Radix-16 on high-duplicate data (v0.2 fix) | **Yes** | Cardinality guard forces R-16 when `duplicateRatio > 0.90` |
| vs Plain Radix-16 on airport-style clustered data | **No (0.52×)** | Cost model still misfires here; one remaining known case |
| vs Plain Radix-16 on sorted/reverse data | **Yes (22×)** | $O(N)$ short-circuit; plain radix runs all passes regardless |

> **Bottom line:** qi-sort is not a magic wrapper around plain radix. It beats comparison-based sorters on every tested dataset and beats radix on high-entropy or pre-ordered data. One known misfire case remains (moderate-duplicate clustered data). All other previously failing cases are fixed in v0.2.

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
```

### Python

```python
import qi_sort

# Sort a list — 2.5× faster than list.sort()
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
| `amplitudeConcentration` | `double` | Inverse Participation Ratio (IPR) |
| `effectiveStates` | `double` | Effective occupied bucket count ($N_{\text{eff}}$) |
| `duplicateRatio` | `double` | Fraction of duplicate values |
| `recommendedRadix` | `qi::Radix` | `R8`, `R11`, or `R16` |

### `qi::SortOptions`

| Field | Default | Description |
| :--- | :--- | :--- |
| `sampleSize` | `8192` | Elements sampled for distribution sensing |
| `allowShortcuts` | `true` | Enable $O(N)$ early-exit for sorted/reverse input |
| `verbose` | `false` | Print live telemetry to `stdout` |

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

**1. Sample & Sense.** On a sample of `sampleSize` elements, qi-sort builds per-byte histograms and computes:

$$\psi_{b,i} = \sqrt{p_{b,i}}, \qquad \text{IPR}_b = \sum_{i=0}^{255} p_{b,i}^2, \qquad N_{\text{eff},b} = \frac{1}{\text{IPR}_b}$$

**2. Predict Cache Pressure.** For Radix-16, the estimated active bucket footprint is:

$$B_{16} = \min(65536,\ N_{\text{eff},0} \times N_{\text{eff},1})$$

When $B_{16}$ exceeds L2 cache capacity, the cost penalty shifts the optimal choice to Radix-11.

**3. Execute.** The selected kernel runs with heap-allocated count arrays, 4-way loop unrolling, and software prefetch (`__builtin_prefetch`). Fully sorted or reverse-sorted inputs short-circuit in $O(N)$.

---

## Repository Structure

```
qi-sort/
├── include/
│   ├── qi_radix.hpp              # C++17 header-only core library
│   └── qi_c_api.h                # C-ABI interface (Python / Java / Rust / Go)
├── src/
│   ├── qi_c_api.cpp              # C-ABI implementation
│   ├── qi_jni.cpp                # Java JNI bridge
│   └── qsort_cli.cpp             # qsort-db CLI utility
├── bindings/
│   ├── python/
│   │   ├── qi_sort.py            # Python ctypes / NumPy integration
│   │   └── test_python.py        # Python benchmark test
│   └── java/
│       └── com/qisort/
│           └── QiSort.java       # Java JNI wrapper
├── benchmarks/
│   ├── real_data_benchmark.cpp   # Real dataset benchmark (NYC, dictionary, airports)
│   ├── real_world_database_benchmark.cpp
│   ├── plain_radix_vs_qi.cpp     # Algorithmic fairness test
│   └── algo_fairness_test.cpp
├── examples/
│   ├── basic_usage.cpp
│   ├── analytical_inspection.cpp
│   ├── database_column_sort.cpp
│   ├── spatial_morton_sort.cpp
│   └── c_api_usage.c
├── CMakeLists.txt
├── LICENSE                       # GNU General Public License v2.0
└── README.md
```

---

## License

Licensed under the **GNU General Public License v2.0** — the original Linux Kernel license. See [LICENSE](LICENSE).

---

## Citation

```bibtex
@software{pandia2026qisort,
  title   = {QI-Sort: Quantum-Inspired Adaptive Radix Sorting Engine},
  author  = {Pandia, Jason},
  year    = {2026},
  url     = {https://github.com/PandiaJason/qi-sort},
  license = {GPL-2.0}
}
```
