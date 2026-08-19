"""
Python Integration Test for QI-Sort
Compares Python built-in sort() vs NumPy vs qi_sort on 1,000,000 integers.
"""

import time
import random
import qi_sort

print("=== QI-Sort Python Integration Test ===")

# Test 1: Small list sorting
data = [10543, 42, 999999, 12, 0, 8881, 100]
print("\nOriginal Python List :", data)
qi_sort.sort(data)
print("Sorted with qi_sort  :", data)

# Test 2: Distribution Analysis
stats = qi_sort.analyze(data)
print("\nData Analysis Stats:")
for k, v in stats.items():
    print(f"  {k}: {v:.4f}")

# Test 3: Benchmark 1,000,000 integer list vs Python sort()
N = 1000000
print(f"\nBenchmark on {N:,} random integers in Python:")

py_data = [random.randint(0, 4294967295) for _ in range(N)]

# Python built-in Timsort
data_timsort = list(py_data)
t0 = time.perf_counter()
data_timsort.sort()
t1 = time.perf_counter()
timsort_time = (t1 - t0) * 1000.0

# QI-Sort Native C++ Execution via C-FFI
data_qisort = list(py_data)
t2 = time.perf_counter()
qi_sort.sort(data_qisort)
t3 = time.perf_counter()
qisort_time = (t3 - t2) * 1000.0

print(f"  * Python list.sort() (Timsort) : {timsort_time:.2f} ms")
print(f"  * qi_sort (Our Native Engine)  : {qisort_time:.2f} ms")
print(f"  * Speedup vs Python list.sort(): {timsort_time / qisort_time:.2f}x FASTER")
print("  * Correctness Verification      :", "PASS" if data_timsort == data_qisort else "FAIL")

# Test 4: NumPy integration (if installed)
try:
    import numpy as np
    print(f"\nNumPy Array Integration ({N:,} elements):")
    np_data = np.random.randint(0, 4294967295, size=N, dtype=np.uint32)

    # NumPy QuickSort
    np_copy = np.copy(np_data)
    t4 = time.perf_counter()
    np_copy.sort()
    t5 = time.perf_counter()
    np_time = (t5 - t4) * 1000.0

    # QI-Sort Direct Buffer NumPy Sorting
    qi_np_copy = np.copy(np_data)
    t6 = time.perf_counter()
    qi_sort.sort(qi_np_copy)
    t7 = time.perf_counter()
    qi_np_time = (t7 - t6) * 1000.0

    print(f"  * NumPy array.sort() (QuickSort) : {np_time:.2f} ms")
    print(f"  * qi_sort on NumPy Direct Buffer : {qi_np_time:.2f} ms")
    print(f"  * Speedup vs NumPy array.sort()  : {np_time / qi_np_time:.2f}x FASTER")
    print("  * Correctness Verification       :", "PASS" if np.array_equal(np_copy, qi_np_copy) else "FAIL")
except ImportError:
    print("\n(NumPy not installed, skipping NumPy zero-copy buffer test)")
