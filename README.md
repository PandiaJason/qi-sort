# QI-Sort: Quantum-Inspired Adaptive Radix Sorting Engine

[![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg)](LICENSE)
[![Language: C++17](https://img.shields.io/badge/Language-C%2B%2B17-green.svg)](include/qi_radix.hpp)
[![Build Status](https://img.shields.io/badge/Build-Passing-brightgreen.svg)](CMakeLists.txt)

**QI-Sort** is a production-grade, C++17 header-only library and high-performance execution engine for ultra-fast adaptive 32-bit integer sorting. It combines **Quantum-Inspired (QI) Probability-Amplitude Distribution Sensing** with cache-aware Radix-8, Radix-11, and Radix-16 execution engines.

---

## 📌 Table of Contents
1. [Overview & Abstract](#-overview--abstract)
2. [Key Features](#-key-features)
3. [Performance Benchmarks](#-performance-benchmarks)
4. [Quick Integration](#-quick-integration)
5. [API Reference](#-api-reference)
6. [Real-World Use Cases & Examples](#-real-world-use-cases--examples)
7. [CLI Software Utility (`qsort-db`)](#-cli-software-utility-qsort-db)
8. [Scientific Theory & Mathematics](#-scientific-theory--mathematics)
9. [License & Citation](#-license--citation)

---

## 📖 Overview & Abstract

Adaptive sorting algorithms dynamically select strategy parameters based on runtime data distributions. However, traditional heuristic classifiers often fail to predict memory hierarchy bottlenecks under varying key entropies. 

**QI-Sort** utilizes a probability-amplitude-inspired state representation ($\psi_{b,i} = \sqrt{p_{b,i}}$) to characterize key distributions at runtime. By evaluating sampled Inverse Participation Ratios ($\text{IPR}_b = \sum |\psi_{b,i}|^4$), effective occupied states ($N_{\text{eff},b} = 1 / \text{IPR}_b$), Shannon entropy ($H_b$), and active bit-masks, QI-Sort dynamically estimates cache-thrashing penalties to dispatch the cost-optimal radix configuration ($\text{Radix-8}$, $\text{Radix-11}$, or $\text{Radix-16}$).

The execution engine combines single-scan dual-histogram generation, software prefetching, heap-allocated 64K counting structures, and $O(N)$ early-exit shortcuts. Enforcing a symmetric experimental methodology where both QI and classical heuristic baselines are strictly charged for analysis and selection overhead ($N = 1,000,000$, 7-trial medians), QI-Sort achieves a **100% correctness pass rate**. Across ten datasets, QI-Sort demonstrates a **mean per-dataset speedup of 2.94× over `std::sort`** (**2.64× aggregate benchmark speedup**) and a **mean per-dataset speedup of 3.38× over the classical adaptive baseline** (**1.64× aggregate benchmark speedup**).

> [!NOTE]
> **Hardware Context Notice:** QI-Sort operates entirely on classical x86_64 / ARM CPU architecture without quantum hardware or quantum simulation. The term "Quantum-Inspired" refers strictly to the mathematical formulation of amplitude-weighted state vectors for feature extraction.

---

## 🚀 Key Features

* **Header-Only & Zero-Dependency:** Requires only a standard C++17 compiler (`g++`, `clang++`, or `MSVC`).
* **Probability-Amplitude Sensing ($\psi_i = \sqrt{p_i}$):** Transforms byte frequency counts into normalized state vectors to compute Inverse Participation Ratio (IPR) and effective active bucket counts.
* **Cache-Thrashing Prevention:** Dynamically predicts when Radix-16 bucket footprints exceed CPU L2 cache capacity (~256KB-512KB) and shifts to Radix-11 or Radix-8 to maintain high cache hit rates.
* **Dual-Histogram Single-Pass Scan:** Computes histograms for both 16-bit passes of Radix-16 in a single contiguous memory iteration over data.
* **Non-Destructive Inspection (`qi::analyze`):** Senses entropy, IPR, effective occupied states ($N_{\text{eff}}$), and duplicate ratios without modifying input memory.
* **Heap-Safe & Prefetched Execution:** Allocates large 64K counting structures on the heap via `std::make_unique` to prevent stack overflow, paired with `__builtin_prefetch` and 4-way loop unrolling.

---

## 📊 Performance Benchmarks

### 1. Synthetic Pipeline Benchmark ($N = 1,000,000$ Elements, 7-Trial Medians)

All timing values below include **State Analysis + Strategy Selection + Radix Sort Execution**.

| Dataset | QI Radix | Class Radix | `std::sort` | Pure R16 (No Shortcuts) | QI Sort Only | QI Total Pipeline | Classical Total Pipeline | Correctness |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **RANDOM** | `RADIX-11` | `RADIX-8` | 20.865 ms | 4.805 ms | **3.271 ms** | **3.435 ms** | 4.698 ms | **PASS** |
| **DUPLICATE-HEAVY** | `RADIX-16` | `RADIX-16` | 5.032 ms | 2.890 ms | 2.918 ms | 3.023 ms | **2.961 ms** | **PASS** |
| **CLUSTERED** | `RADIX-16` | `RADIX-16` | 9.271 ms | 3.142 ms | 3.183 ms | 3.366 ms | **3.171 ms** | **PASS** |
| **SORTED** | `RADIX-16` | `RADIX-11` | 0.965 ms | 8.020 ms | 0.311 ms | 0.416 ms | **0.334 ms** | **PASS** |
| **REVERSE** | `RADIX-16` | `RADIX-11` | 1.669 ms | 8.048 ms | **0.390 ms** | **0.511 ms** | 11.330 ms | **PASS** |
| **LOW-RANGE** | `RADIX-16` | `RADIX-16` | 18.050 ms | 3.936 ms | 4.017 ms | 4.231 ms | **4.015 ms** | **PASS** |
| **ALTERNATING** | `RADIX-16` | `RADIX-16` | 3.106 ms | 4.996 ms | 4.940 ms | 5.060 ms | **4.962 ms** | **PASS** |
| **ALMOST-SORTED** | `RADIX-16` | `RADIX-11` | 4.442 ms | 7.989 ms | **8.081 ms** | **8.280 ms** | 11.415 ms | **PASS** |
| **POWER-DISTRIBUTION** | `RADIX-16` | `RADIX-11` | 17.395 ms | 4.205 ms | **4.253 ms** | **4.500 ms** | 6.133 ms | **PASS** |
| **SAWTOOTH** | `RADIX-16` | `RADIX-11` | 16.701 ms | 3.847 ms | **3.864 ms** | **4.099 ms** | 11.699 ms | **PASS** |

#### Speedup Summary:
* **Vs. `std::sort` (Introsort):** **`2.94x` Mean Per-Dataset Speedup** | **`2.64x` Aggregate Speedup**
* **Vs. Classical Heuristic Baseline:** **`3.38x` Mean Per-Dataset Speedup** | **`1.64x` Aggregate Speedup**

---

### 2. Real-World Database Column Benchmark ($N = 10,000,000$ Rows / 40M Elements)

Simulates an in-memory columnar database query engine (DuckDB / ClickHouse style) executing an `ORDER BY` query on 10 Million rows per column:

| Column Name | Real-World Data Pattern | `std::sort` | `std::stable_sort` | `qi::sort` (Our Engine) | Speedup vs `std::sort` |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **`order_id`** | Transaction Order IDs (Uniform 32-bit) | 229.43 ms | 265.74 ms | **135.49 ms** | **1.69x** |
| **`user_id`** | Customer Keys (Power-Law Clustered) | 113.94 ms | 291.05 ms | **54.93 ms** | **2.07x** |
| **`timestamp_sec`** | Event Epochs (Monotonic / Almost Sorted) | 61.99 ms | 301.98 ms | **74.21 ms** | 0.83x |
| **`category_code`** | Product Categories (Low-Range 16-bit) | 133.63 ms | 257.49 ms | **68.14 ms** | **1.96x** |
| **FULL TABLE TOTAL** | **40,000,000 Total Elements** | **538.98 ms (0.54s)** | **1,116.26 ms (1.12s)** | **332.76 ms (0.33s)** | **`1.62x` FASTER** |

---

## 📦 Quick Integration

Simply copy [`include/qi_radix.hpp`](file:///Users/admin/Jas%20Apps/QSORT/include/qi_radix.hpp) into your project's include directory:

```cpp
#include "qi_radix.hpp"

// 1. Sort a std::vector<uint32_t>
std::vector<uint32_t> numbers = {10543, 42, 999999, 12, 0, 8881};
qi::sort(numbers);

// 2. Sort a raw C-style array or pointer range
uint32_t arr[6] = {500, 20, 100, 5, 200, 10};
qi::sort(arr, 6);

// 3. Sort via C++ iterator range
qi::sort(numbers.begin(), numbers.end());
```

---

## 🛠 API Reference

### `qi::sort`
```cpp
void qi::sort(std::vector<uint32_t>& data, qi::SortOptions options = {});
void qi::sort(uint32_t* data, size_t n, qi::SortOptions options = {});

template <typename RandomIt>
void qi::sort(RandomIt begin, RandomIt end, qi::SortOptions options = {});
```
Sorts 32-bit unsigned integer data in ascending order using quantum-inspired distribution sensing and adaptive radix dispatch.

### `qi::analyze`
```cpp
qi::State qi::analyze(const std::vector<uint32_t>& data, size_t sampleSize = 8192);
qi::State qi::analyze(const uint32_t* data, size_t n, size_t sampleSize = 8192);
```
Performs non-destructive statistical state vector analysis on data without modifying memory.

### `qi::SortOptions`
* `size_t sampleSize = 8192`: Number of elements sampled for state sensing.
* `bool allowShortcuts = true`: Enables $O(N)$ early exit detection for pre-sorted/reverse sequences.
* `bool verbose = false`: Prints real-time telemetry diagnostics to `std::cout`.

---

## 💡 Real-World Use Cases & Examples

### 1. Columnar Database Query Engines ([`examples/database_column_sort.cpp`](file:///Users/admin/Jas%20Apps/QSORT/examples/database_column_sort.cpp))
Accelerating `ORDER BY` and `GROUP BY` surrogate join keys in zero-copy columnar buffers (DuckDB, ClickHouse, Apache Arrow).

### 2. 3D Graphics & GPU Ray Tracing ([`examples/spatial_morton_sort.cpp`](file:///Users/admin/Jas%20Apps/QSORT/examples/spatial_morton_sort.cpp))
Sorting 30-bit Morton Z-order curve keys for real-time Bounding Volume Hierarchy (BVH) construction in game engines.

### 3. Non-Destructive Distribution Inspection ([`examples/analytical_inspection.cpp`](file:///Users/admin/Jas%20Apps/QSORT/examples/analytical_inspection.cpp))
Extracting Shannon entropy, IPR, effective occupied buckets ($N_{\text{eff}}$), and duplicate ratios.

---

## 💻 CLI Software Utility (`qsort-db`)

The project includes a production command-line tool [`src/qsort_cli.cpp`](file:///Users/admin/Jas%20Apps/QSORT/src/qsort_cli.cpp) for processing binary disk datasets:

```bash
# Build CLI tool
g++ -O3 -std=c++17 -march=native src/qsort_cli.cpp -o qsort-db

# Generate a 25,000,000 integer binary file (95.37 MB) on disk:
./qsort-db --generate 25000000 data_100mb.bin

# Benchmark real binary file sorting (reports MKeys/sec throughput):
./qsort-db --benchmark data_100mb.bin
```

---

## 📐 Scientific Theory & Mathematics

### A. Probability-Amplitude State Vector
Given sample frequency counts $C_{b,i}$ for byte $b \in \{0,1,2,3\}$ and symbol $i \in [0, 255]$:

$$p_{b,i} = \frac{C_{b,i}}{M}$$

$$\psi_{b,i} = \sqrt{p_{b,i}}, \quad \text{where} \quad \sum_{i=0}^{255} |\psi_{b,i}|^2 = 1$$

### B. Inverse Participation Ratio (IPR) & Effective States
Measuring state vector concentration:

$$\text{IPR}_b = \sum_{i=0}^{255} |\psi_{b,i}|^4 = \sum_{i=0}^{255} p_{b,i}^2$$

$$N_{\text{eff},b} = \frac{1}{\text{IPR}_b} = \frac{1}{\sum_{i=0}^{255} p_{b,i}^2}$$

### C. Cache-Thrashing Cost Function
Estimating execution cost $C(r)$ for candidate radix size $r \in \{8, 11, 16\}$:

$$C(r) = N \cdot \left[ P(r) \cdot C_{\text{pass}} + \text{Penalty}_{\text{cache}}(r, \text{QI}) \right]$$

For Radix-16 ($65,536$ buckets), when effective active buckets $B_{16} = \min(65536, N_{\text{eff},0} \times N_{\text{eff},1})$ exceed L2 cache capacity (~32,768 size_t entries), the penalty shifts the dispatch to **Radix-11**, preventing cache thrashing.

---

## 📜 License & Citation

This project is licensed under the **GNU General Public License v2.0 (GPL-2.0)** — the original Linux Kernel License. See [LICENSE](LICENSE) for details.

### Citation
```bibtex
@article{pandia2026qisort,
  title={QI-Sort: Probability-Amplitude-Inspired Distribution Analysis for Adaptive Radix Sorting},
  author={Pandia, Jason},
  journal={GitHub Repository},
  url={https://github.com/PandiaJason/qi-sort},
  year={2026}
}
```
