<div align="center">

<h1>⚡ QI-Sort</h1>

<p><strong>Quantum-Inspired Adaptive Radix Sorting Engine</strong></p>

<p>A production-grade, header-only C++17 library that sorts 32-bit integers up to <strong>4.73× faster than <code>std::sort</code></strong>.<br/>
Native bindings for <strong>Python</strong>, <strong>Java</strong>, and <strong>C</strong> included.</p>

[![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg?style=flat-square)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=c%2B%2B)](include/qi_radix.hpp)
[![Python](https://img.shields.io/badge/Python-3.7%2B-3776AB?style=flat-square&logo=python&logoColor=white)](bindings/python/qi_sort.py)
[![Java](https://img.shields.io/badge/Java-JNI-ED8B00?style=flat-square&logo=openjdk&logoColor=white)](bindings/java/com/qisort/QiSort.java)
[![Header Only](https://img.shields.io/badge/header--only-yes-brightgreen?style=flat-square)](include/qi_radix.hpp)
[![Zero Dependencies](https://img.shields.io/badge/dependencies-zero-success?style=flat-square)](include/qi_radix.hpp)

</div>

---

## Overview

QI-Sort uses **Probability-Amplitude Distribution Sensing** to characterize integer datasets at runtime. By computing Inverse Participation Ratio (IPR) and effective active states ($N_{\text{eff}}$) from sampled byte frequencies, it dynamically selects the optimal radix execution kernel (Radix-8, Radix-11, or Radix-16) while preventing CPU L2 cache thrashing.

The result: a sorting engine that consistently outperforms both `std::sort` and `std::stable_sort` across diverse real-world data distributions.

---

## Benchmarks

### Real File I/O Benchmark — `qsort-db` CLI (25M keys, 95 MB binary file)

| Algorithm | Time | Throughput | vs `std::sort` |
| :--- | ---: | ---: | :---: |
| `std::stable_sort` | 814.91 ms | 30.7 MKeys/s | 0.73× |
| `std::sort` | 597.33 ms | 41.9 MKeys/s | baseline |
| **`qi::sort`** | **126.16 ms** | **198.2 MKeys/s** | **4.73× faster** |

---

### Python Benchmark — 1,000,000 integer list

| Algorithm | Time | vs Python `list.sort()` |
| :--- | ---: | :---: |
| `list.sort()` (Timsort) | 299.61 ms | baseline |
| **`qi_sort.sort()`** | **119.65 ms** | **2.50× faster** |

---

### Columnar Database Benchmark — 10M rows per column (40M total)

| Column | Distribution | `std::sort` | `std::stable_sort` | `qi::sort` | Speedup |
| :--- | :--- | ---: | ---: | ---: | :---: |
| `order_id` | Uniform 32-bit | 229.43 ms | 265.74 ms | **135.49 ms** | 1.69× |
| `user_id` | Power-Law Clustered | 113.94 ms | 291.05 ms | **54.93 ms** | 2.07× |
| `category_code` | Low-Range 16-bit | 133.63 ms | 257.49 ms | **68.14 ms** | 1.96× |
| **Total** | 40M elements | **0.54 s** | **1.12 s** | **0.33 s** | **1.62×** |

All benchmarks run on a single core with `-O3 -march=native`. Results verified with 100% correctness against `std::sort` reference output.

---

## Installation

### C++ — Header Only

Copy [`include/qi_radix.hpp`](include/qi_radix.hpp) into your project. No dependencies, no build step.

```bash
cp include/qi_radix.hpp /your/project/include/
```

### CMake

```cmake
add_subdirectory(path/to/qi-sort)
target_link_libraries(your_target PRIVATE qi_radix)
```

### Build Shared Library (for Python / Java / C bindings)

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

// std::vector
std::vector<uint32_t> data = {10543, 42, 999999, 12, 0, 8881};
qi::sort(data);

// Raw pointer range
uint32_t arr[] = {500, 20, 100, 5, 200, 10};
qi::sort(arr, 6);

// Iterator range
qi::sort(data.begin(), data.end());
```

### Python

```bash
# Copy bindings/python/qi_sort.py into your project, alongside libqisort.dylib / .so
```

```python
import qi_sort

# Sort a Python list — 2.5× faster than list.sort()
data = [10543, 42, 999999, 12, 0, 8881]
qi_sort.sort(data)
print(data)  # [0, 12, 42, 8881, 10543, 999999]

# Sort a NumPy array directly (zero-copy buffer access)
import numpy as np
arr = np.random.randint(0, 2**32, size=1_000_000, dtype=np.uint32)
qi_sort.sort(arr)

# Non-destructive distribution inspection
stats = qi_sort.analyze(data)
print(stats)
# {'entropy': 0.1582, 'ipr': 0.5714, 'effective_states': 2.9758, 'duplicate_ratio': 0.0}
```

### Java (JNI)

```bash
# Ensure libqisort.dylib / libqisort.so is accessible at runtime
```

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

double entropy, ipr, neff, dup_ratio;
qi_analyze_u32(data, 6, &entropy, &ipr, &neff, &dup_ratio);
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

### `qi::analyze`

Non-destructive statistical state vector analysis. Does **not** modify data.

```cpp
qi::State qi::analyze(const std::vector<uint32_t>& data, size_t sampleSize = 8192);
qi::State qi::analyze(const uint32_t* data, size_t n, size_t sampleSize = 8192);
```

**Returns `qi::State`:**

| Field | Type | Description |
| :--- | :--- | :--- |
| `averageEntropy` | `double` | Shannon entropy across all byte lanes |
| `amplitudeConcentration` | `double` | Inverse Participation Ratio (IPR) |
| `effectiveStates` | `double` | Effective occupied bucket count ($N_{\text{eff}}$) |
| `duplicateRatio` | `double` | Fraction of duplicate values (0.0–1.0) |
| `recommendedRadix` | `qi::Radix` | `R8`, `R11`, or `R16` |

### `qi::SortOptions`

| Field | Default | Description |
| :--- | :--- | :--- |
| `sampleSize` | `8192` | Elements sampled for state sensing |
| `allowShortcuts` | `true` | Enable $O(N)$ early-exit for sorted / reverse input |
| `verbose` | `false` | Print live telemetry to `stdout` |

---

## CLI Utility — `qsort-db`

A production binary for benchmarking real disk files.

```bash
# Build
g++ -O3 -std=c++17 -march=native src/qsort_cli.cpp -o qsort-db

# Generate a 95 MB test dataset
./qsort-db --generate 25000000 data.bin

# Run a full three-way benchmark on the file
./qsort-db --benchmark data.bin
```

Sample output:
```
Algorithm                Time (ms)    Throughput (MKeys/s)    Speedup
────────────────────────────────────────────────────────────────────────
std::sort                597.33       41.85                   1.00x (Baseline)
std::stable_sort         814.91       30.68                   0.73x
qi::sort (Our Engine)    126.16       198.16                  4.73x FASTER
────────────────────────────────────────────────────────────────────────
Correctness Verification : PASS (100% Exact Match)
```

---

## Where to Use QI-Sort

| Domain | Application |
| :--- | :--- |
| **Columnar Databases** | `ORDER BY` / `GROUP BY` on integer surrogate keys (DuckDB, ClickHouse, Arrow) |
| **Search Engines** | Posting list and document ID sorting in inverted index pipelines |
| **3D Graphics / Games** | Morton Z-order BVH tree construction for ray tracing |
| **Network Telemetry** | High-frequency IP flow ID and packet timestamp ordering |
| **Scientific Computing** | Parallel radix histogram sorts in simulation pipelines |
| **High-Frequency Trading** | Nanosecond-timestamped order book event sorting |

---

## Repository Structure

```
qi-sort/
├── include/
│   ├── qi_radix.hpp        # C++17 header-only core library
│   └── qi_c_api.h          # C-ABI interface for cross-language FFI
├── src/
│   ├── qi_c_api.cpp        # C-ABI implementation
│   ├── qi_jni.cpp          # Java JNI bridge
│   └── qsort_cli.cpp       # qsort-db CLI utility
├── bindings/
│   ├── python/
│   │   ├── qi_sort.py      # Python ctypes / NumPy integration
│   │   └── test_python.py  # Python benchmark & test
│   └── java/
│       └── com/qisort/
│           └── QiSort.java # Java JNI wrapper class
├── benchmarks/
│   └── real_world_database_benchmark.cpp
├── examples/
│   ├── basic_usage.cpp
│   ├── analytical_inspection.cpp
│   ├── database_column_sort.cpp
│   ├── spatial_morton_sort.cpp
│   └── c_api_usage.c
├── CMakeLists.txt
├── LICENSE                 # GNU General Public License v2.0
└── README.md
```

---

## How It Works

QI-Sort transforms sampled byte frequency counts into probability amplitude magnitudes:

$$\psi_{b,i} = \sqrt{p_{b,i}}, \qquad \sum_{i=0}^{255} |\psi_{b,i}|^2 = 1$$

It quantifies state vector concentration using **Inverse Participation Ratio (IPR)**:

$$\text{IPR}_b = \sum_{i=0}^{255} p_{b,i}^2, \qquad N_{\text{eff},b} = \frac{1}{\text{IPR}_b}$$

When the effective bucket footprint of Radix-16 ($B_{16} = N_{\text{eff},0} \times N_{\text{eff},1}$) exceeds L2 cache capacity, QI-Sort shifts execution to Radix-11, eliminating cache-thrashing misses. Fully sorted and reverse-sorted inputs are detected in $O(N)$ with an immediate short-circuit return.

---

## License

Licensed under the **GNU General Public License v2.0** — the original Linux Kernel license.  
See [LICENSE](LICENSE) for full terms.

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
