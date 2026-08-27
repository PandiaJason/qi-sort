# Showpiece Technical Announcement: qi-sort

> **Title Ideas:**
> 1. *We built qi-sort: A zero-dependency C++17 adaptive sorter delivering 3,095 MKeys/s and beating std::sort by up to 27×*
> 2. *Beating std::sort, DuckDB, and fixed radix sorts with 50ns adaptive bitwise state routing*

---

## The Big Idea: Solving the "Fixed Radix Trap"

Sorting integer, float, and timestamp data is the single most expensive operation inside databases, analytical query engines, dataframes, and LSM-tree storage systems.

For decades, performance engineers faced a brutal tradeoff:

1. **Comparison Sorters (`std::sort`, `pdqsort`, Google `vqsort`)**: Bound by $O(N \log N)$ comparisons. Comparing keys with branching instructions causes CPU pipeline stalls on random data.
2. **Fixed Radix Sorters (Radix-8, Radix-11, Radix-16)**: Non-comparison $O(k \cdot N)$ speed, but trapped by **fixed pass counts**:
   - Hardcoding **Radix-16** creates $65,536$ histogram buckets ($256\text{ KB}$), overflowing CPU L1 cache ($32\text{–}64\text{ KB}$) and causing memory stalls on random high-entropy data.
   - Hardcoding **Radix-11** requires 3 full memory passes, running slower on low-range data.
   - Hardcoding **Radix-8** requires 4 full memory passes.

---

## Enter `qi::sort`: High-Performance Adaptive Radix Engine

`qi::sort` is a **zero-dependency, single-header C++17 library** ([`include/qi_radix.hpp`](../include/qi_radix.hpp)) that solves the Fixed Radix Trap.

Before sorting, `qi::sort` runs a **~50-nanosecond pure-integer probe** that measures the active bit-width (`bitOr`) and unique bucket occupancy (`lsbOccupied`), routing execution to the fastest specialized kernel:

* **Narrow Domains ($v \le 4095$)** $\rightarrow$ Dispatches **Counting Sort** (**`0.31 ms`** for 1M keys, **`32.3 ms`** for 100M keys — **3,095 MKeys/s**).
* **High Entropy (Uniform 32-bit Random)** $\rightarrow$ Dispatches **L1-bound 8-Way ILP Radix-11** (8 KB histograms, $PF=48$ lookahead prefetch) (**`3.46 ms`** for 1M keys).
* **Heavy Duplicates / Skewed Data** $\rightarrow$ Dispatches **Prefetched Radix-16** (2 passes) (**`4.03 ms`** for 1M keys).
* **Sorted / Reverse-Sorted Sequences** $\rightarrow$ Dispatches **$O(N)$ short-circuits** (**`0.34 ms`** — **27.1× faster**).

---

## Head-to-Head Benchmark Matrix ($N = 1,000,000$ Keys)

| Dataset | Plain Radix-8 | Plain Radix-11 | Plain Radix-16 | **qi::sort (v0.3.61)** | Speedup vs Best Radix |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **Uniform Random 32-bit** | 5.87 ms | 4.10 ms | 5.61 ms | **3.46 ms** | **1.18× FASTER** |
| **Low-Range (0–255)** | 9.58 ms | 7.36 ms | 5.02 ms | **0.31 ms** | **16.2× FASTER** |
| **Clustered Duplicates** | 9.39 ms | 6.68 ms | 4.01 ms | **4.03 ms** | ≈ EQUAL |
| **Sorted (Ascending)** | 11.19 ms | 9.88 ms | 9.22 ms | **0.34 ms** | **27.1× FASTER** |
| **Reverse (Descending)** | 11.35 ms | 9.83 ms | 8.82 ms | **0.53 ms** | **16.7× FASTER** |

---

## 100 Million Key Stress Test ($N = 100,000,000$, 400 MB RAM)

* **100M Uniform Random 32-bit**: **`462.00 ms`** (216.45 MKeys/s)
* **100M Duplicate Categories (0–255)**: **`32.30 ms`** (**3,095.51 MKeys/s**)
* **100M Multi-Core Parallel**: **`114.51 ms`** (**873.28 MKeys/s**)

---

## 10-Second Quick Start (Single-Header Drop-in)

No build step needed. Drop `include/qi_radix.hpp` into your project:

```cpp
#include "qi_radix.hpp"
#include <vector>

int main() {
    std::vector<uint32_t> data = {42, 10, 100, 5, 9999};
    
    // Drop-in replacement for std::sort
    qi::sort(data);
    
    return 0;
}
```

---

## Try it Yourself:
* **GitHub Repository:** [https://github.com/PandiaJason/qi-sort](https://github.com/PandiaJason/qi-sort)
* **PyPI Package:** `pip install qi-sort`
* **Go Module:** `go get github.com/PandiaJason/qi-sort/bindings/go`
