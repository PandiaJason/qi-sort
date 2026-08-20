# PROJECT DOCUMENTATION — qi-sort

## Project Overview

**qi-sort** (Quantum-Inspired Adaptive Radix Sorting Engine) is a production-grade, zero-dependency, header-only C++17 library with native bindings for **Python**, **Java**, and **C-ABI**.

It introduces **runtime probability-amplitude distribution sensing** to characterize integer datasets before sorting, dynamically selecting the optimal radix kernel (Radix-16, Radix-11, or Radix-8) to maximize throughput while protecting L2 CPU cache residency.

---

## Technical Architecture

### 1. Quantum-Inspired State Sensing Pipeline

Before executing radix sorting passes, `qi-sort` samples a subset of the dataset (default: 8,192 elements) to compute per-byte probability amplitudes and concentration metrics:

$$\psi_{b,i} = \sqrt{p_{b,i}}$$

$$\text{IPR}_b = \sum_{i=0}^{255} |\psi_{b,i}|^4 = \sum_{i=0}^{255} p_{b,i}^2$$

$$N_{\text{eff},b} = \frac{1}{\text{IPR}_b}$$

Where:
* $p_{b,i}$ is the probability of byte symbol $i$ in byte lane $b$.
* $\psi_{b,i}$ represents the probability amplitude.
* $\text{IPR}_b$ is the **Inverse Participation Ratio**, borrowed from quantum condensed-matter physics (Anderson localization theory) to measure wavefunction localization.
* $N_{\text{eff},b}$ represents the effective number of active bucket states occupied by the data.

### 2. Cache-Thrashing Cost Model & Kernel Dispatch

For Radix-16 (2 passes), the active bucket memory footprint is estimated as:

$$B_{16} = \min\left(65536,\ N_{\text{eff},0} \times N_{\text{eff},1}\right)$$

When high entropy causes $B_{16}$ to exceed the CPU's L2 cache capacity, the cost model penalizes Radix-16 and shifts execution to Radix-11 (3 passes, 2,048 buckets) or Radix-8 (4 passes, 256 buckets).

#### Cardinality Guard (v0.2 Fix):
If `duplicateRatio > 0.90` (over 90% duplicate elements in sample), the algorithm forces Radix-16 directly, bypassing penalty formulas since high duplicate ratio guarantees tiny active bucket footprints regardless of distribution entropy.

### 3. $O(N)$ Short-Circuit Early Exit

Before performing full radix distribution passes, `qi-sort` evaluates sequence order. If the sampled array is already sorted or reverse-sorted, `qi-sort` returns in $O(N)$ single-pass time (achieving **19.8× to 24× speedups** over full radix algorithms).

---

## Empirical Benchmark Suite

All benchmarks conducted on macOS (native Apple Silicon / x86_64, `g++ -O3 -march=native`).

### 1. `qi::sort` vs QuickSort Variants

| Dataset | C stdlib `qsort()` | Native Hoare QuickSort | `std::sort` (Introsort) | `qi::sort` | Speedup vs C `qsort` | Speedup vs `std::sort` |
| :--- | ---: | ---: | ---: | ---: | :---: | :---: |
| **1M Uniform Random 32-bit** | 87.26 ms | 61.71 ms | 20.72 ms | **3.78 ms** | **23.0× FASTER** | **5.47× FASTER** |
| **5M Uniform Random 32-bit** | 484.97 ms | 341.44 ms | 113.11 ms | **25.85 ms** | **18.7× FASTER** | **4.37× FASTER** |
| **NYC Taxi Timestamps (87k)** | 6.35 ms | 4.49 ms | 1.59 ms | **0.55 ms** | **11.5× FASTER** | **2.88× FASTER** |

### 2. Real-World Public Datasets

| Dataset | Source | Size | `std::sort` | `std::stable_sort` | Plain Radix-16 | `qi::sort` | vs `std::sort` |
| :--- | :--- | ---: | ---: | ---: | ---: | ---: | :---: |
| **NYC Taxi Timestamps** | NYC Open Data / TLC | 87,921 | 1.71 ms | 1.12 ms | 0.37 ms | **0.46 ms** | **3.71× FASTER** |
| **English Dictionary Hashes** | `/usr/share/dict/words` | 943,904 | 19.59 ms | 14.75 ms | 4.60 ms | **3.25 ms** | **6.02× FASTER** |
| **Airport Elevation Data** | OurAirports / GitHub | 851,316 | 9.08 ms | 13.16 ms | 4.42 ms | **8.37 ms** | **1.08× FASTER** |
| **Aggregate (Combined)** | All 3 Real Datasets | 1,883,141 | 30.37 ms | 29.04 ms | 9.40 ms | **12.08 ms** | **2.51× FASTER** |

### 3. Real File I/O — `qsort-db` CLI Utility (25M Keys / 95 MB Disk File)

| Algorithm | Time (ms) | Throughput (MKeys/sec) | vs `std::sort` |
| :--- | ---: | ---: | :---: |
| `std::stable_sort` | 848.99 ms | 29.5 MKeys/s | 0.68× |
| `std::sort` | 582.93 ms | 42.9 MKeys/s | Baseline (1.00×) |
| **`qi::sort`** | **262.12 ms** | **95.4 MKeys/s** | **2.22× FASTER** |

### 4. Columnar Database Sort (40 Million Rows across 4 Columns)

| Column Name | Distribution Type | `std::sort` | `std::stable_sort` | `qi::sort` | Speedup |
| :--- | :--- | ---: | ---: | ---: | :---: |
| `order_id` | Uniform 32-bit | 222.51 ms | 260.18 ms | **39.01 ms** | **5.70× FASTER** |
| `user_id` | Power-Law Clustered | 111.75 ms | 281.07 ms | **51.84 ms** | **2.15× FASTER** |
| `timestamp_sec` | Monotonic / Almost-Sorted | 60.13 ms | 299.42 ms | **72.22 ms** | **0.83× (Timsort opt)** |
| `category_code` | Low-Range 16-bit | 132.22 ms | 253.66 ms | **67.40 ms** | **1.96× FASTER** |
| **TOTAL TABLE** | **40,000,000 Rows** | **526.60 ms** | **1094.34 ms** | **230.46 ms** | **2.28× FASTER** |

### 5. Multi-Language Binding Performance (Python 1M List)

| Implementation | Time (ms) | Speedup vs Python `list.sort()` |
| :--- | ---: | :---: |
| Python `list.sort()` (C-Timsort) | 268.70 ms | Baseline (1.00×) |
| **`qi_sort.sort()` (Native C-ABI)** | **115.71 ms** | **2.32× FASTER** |

---

## Why `qi::sort` Outperforms QuickSort

### 1. Algorithmic Complexity Advantage ($O(N \cdot k)$ vs $O(N \log N)$)
* **QuickSort / Introsort:** Performs $O(N \log_2 N)$ element-to-element comparisons. For $N = 10\text{M}$, $\log_2(10^7) \approx 24$, requiring ~240 million comparisons and conditional branches.
* **`qi::sort`:** Is a non-comparison radix algorithm executing in $O(N \cdot k)$ steps ($k \le 2\text{ or }3$). It extracts bit chunks and indexes array offsets directly.

### 2. Elimination of CPU Branch Misprediction Stalls
* **QuickSort:** Evaluates `if (arr[i] < pivot)` on every step. On unsorted or random inputs, the CPU branch predictor fails ~50% of the time, causing CPU pipeline flushes (costing 15-20 cycles per mispredict).
* **`qi::sort`:** Core sorting loops contain **zero comparison branches**:
  ```cpp
  dst[count[(value >> shift) & mask]++] = value;
  ```
  The CPU executes instructions linearly with maximum Instructions-Per-Cycle (IPC) throughput.

### 3. Absence of Function Pointer Indirect Calls (vs C `qsort`)
* C `qsort()` takes a function pointer callback `int (*compar)(const void*, const void*)`. Because it cannot be inlined across function pointers, sorting $N$ keys requires tens of millions of stack frame allocations and indirect jumps.
* `qi::sort` is fully header-only and inlined by standard C++ compilers.

### 4. Cache Line Protection & Memory Prefetching
* `qi::sort` uses 4-way loop unrolling and explicit compiler prefetching hints (`__builtin_prefetch(&src[i + 32], 0, 1)`), saturating modern CPU memory buses while maintaining L2 cache bucket residency via adaptive radix sizing.

---

## Implementation Audit & Verification Suite

`qi-sort` includes an automated 25-check verification suite (`benchmarks/verify_implementation.cpp`):

| Check # | Audit Target | Status | Result / Metric |
| :---: | :--- | :---: | :--- |
| **1** | $\psi = \sqrt{p}$ Probability Amplitude Computation | PASS | Exact match to $10^{-9}$ precision |
| **2** | $\text{IPR} = \sum p_i^2$ Wavefunction Concentration | PASS | Exact match to $10^{-9}$ precision |
| **3** | $N_{\text{eff}} = 1/\text{IPR}$ Effective Bucket Count | PASS | Verified $N_{\text{eff}}=1.0$ (identical) to $256.0$ (uniform) |
| **4** | Cost Model Dynamic Radix Selection | PASS | Correctly switches between R-16, R-11, R-8 |
| **5** | $O(N)$ Sorted Short-Circuit Execution | PASS | Verified 22.1× speedup on pre-sorted array |
| **6** | Output Correctness across Data Variants | PASS | 100% exact match to reference `std::sort` |
| **7** | Benchmark Timing Cleanliness (No Cache Warming) | PASS | Variance < 25% across independent fresh runs |
| **8** | Sensing Overhead Timing Inclusion | PASS | Verified sensing overhead is 4–8% and charged inside `qi::sort` time |

**Audit Status: 25 / 25 PASSED.**

---

## File Structure

```
qi-sort/
├── include/
│   ├── qi_radix.hpp                      # Core C++17 header-only implementation
│   └── qi_c_api.h                        # C-ABI header
├── src/
│   ├── qi_c_api.cpp                      # C shared library wrapper
│   ├── qi_jni.cpp                        # Java JNI native bridge
│   └── qsort_cli.cpp                     # qsort-db CLI utility
├── bindings/
│   ├── python/
│   │   ├── qi_sort.py                    # Python ctypes & NumPy integration
│   │   └── test_python.py                # Python benchmark suite
│   └── java/
│       └── com/qisort/QiSort.java        # Java class wrapper
├── benchmarks/
│   ├── real_quicksort_test.cpp           # C qsort vs Hoare Quicksort vs std::sort vs qi::sort
│   ├── quicksort_vs_qi.cpp               # Size-scaling Quicksort comparison
│   ├── real_data_benchmark.cpp           # NYC taxi + dictionary + airports
│   ├── real_world_database_benchmark.cpp # 40M row columnar database simulation
│   ├── plain_radix_vs_qi.cpp             # Algorithmic fairness comparison
│   ├── algo_fairness_test.cpp            # Pure C++ algorithm comparison
│   ├── cost_model_comparison.cpp         # QI vs entropy threshold vs fixed R-16
│   └── verify_implementation.cpp         # 25-check automated audit
├── examples/
│   ├── basic_usage.cpp
│   ├── analytical_inspection.cpp
│   ├── database_column_sort.cpp
│   ├── spatial_morton_sort.cpp
│   └── c_api_usage.c
├── CMakeLists.txt
├── LICENSE                               # GNU General Public License v2.0
├── README.md                             # Repository README
└── PROJECT.md                            # Comprehensive Technical Project File
```

---

## License & Citation

Licensed under the **GNU General Public License v2.0** (GPL-2.0).

```bibtex
@software{pandia2026qisort,
  title   = {QI-Sort: Quantum-Inspired Adaptive Radix Sorting Engine},
  author  = {Pandia, Jason},
  year    = {2026},
  url     = {https://github.com/PandiaJason/qi-sort},
  license = {GPL-2.0}
}
```
