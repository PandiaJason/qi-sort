"""
===============================================================================
PYTHON BENCHMARK MATRIX: QI_SORT vs ALL PYTHON SORTING ENGINES
===============================================================================
Compares qi_sort directly against all standard Python sorting mechanisms & engines:

  1. qi_sort.sort(np_array)       (qi::sort Zero-Copy C-Pointer on NumPy)
  2. qi_sort.sort(py_list)        (qi::sort C-Buffer on Python List)
  3. list.sort()                  (Built-in Python C-Timsort in-place)
  4. sorted()                     (Built-in Python C-Timsort copy)
  5. numpy.sort(kind='quicksort') (NumPy C-QuickSort)
  6. numpy.sort(kind='mergesort') (NumPy C-MergeSort)
  7. numpy.sort(kind='heapsort')   (NumPy C-HeapSort)
  8. numpy.sort(kind='stable')    (NumPy C-StableSort)
  9. pandas.Series.sort_values()  (Pandas Series Column Sorter)

Dataset Size: N = 1,000,000 32-bit Integers per distribution.
===============================================================================
"""

import time
import random
import numpy as np
import pandas as pd
import qi_sort

def run_benchmark():
    N = 1000000 # 1 Million items
    distributions = {
        "1. Uniform Random": [random.randint(0, 4294967295) for _ in range(N)],
        "2. Heavy Duplicates (0-255)": [random.randint(0, 255) for _ in range(N)],
        "3. Hash Join Keys": [(i * 2654435761) % 4294967295 for i in range(N)],
        "4. Nearly Sorted (95% Ordered)": list(range(N))
    }
    
    # Introduce small perturbations for nearly sorted
    for i in range(10000):
        idx1, idx2 = random.randint(0, N-1), random.randint(0, N-1)
        distributions["4. Nearly Sorted (95% Ordered)"][idx1], distributions["4. Nearly Sorted (95% Ordered)"][idx2] = \
        distributions["4. Nearly Sorted (95% Ordered)"][idx2], distributions["4. Nearly Sorted (95% Ordered)"][idx1]

    print("=" * 110)
    print("  PYTHON BENCHMARK MATRIX: QI_SORT vs ALL PYTHON SORTING ENGINES (N = 1,000,000 Keys)")
    print("=" * 110 + "\n")

    for dist_name, raw_list in distributions.items():
        print("-" * 110)
        print(f" DATASET: {dist_name}")
        print("-" * 110)
        
        results = {}

        # 1. qi_sort.sort(np_array) -> Zero Copy
        np_arr = np.array(raw_list, dtype=np.uint32)
        s = time.perf_counter()
        qi_sort.sort(np_arr)
        t_qi_np = (time.perf_counter() - s) * 1000.0
        results["qi_sort.sort(np_array) [Ours]"] = t_qi_np

        # 2. qi_sort.sort(py_list) -> Python List
        c_list = list(raw_list)
        s = time.perf_counter()
        qi_sort.sort(c_list)
        t_qi_list = (time.perf_counter() - s) * 1000.0
        results["qi_sort.sort(py_list) [Ours]"] = t_qi_list

        # 3. list.sort() -> C-Timsort
        c_list = list(raw_list)
        s = time.perf_counter()
        c_list.sort()
        t_list = (time.perf_counter() - s) * 1000.0
        results["list.sort() (Python C-Timsort in-place)"] = t_list

        # 4. sorted() -> C-Timsort Copy
        c_list = list(raw_list)
        s = time.perf_counter()
        _ = sorted(c_list)
        t_sorted = (time.perf_counter() - s) * 1000.0
        results["sorted() (Python C-Timsort copy)"] = t_sorted

        # 5. numpy.sort(quicksort)
        np_arr = np.array(raw_list, dtype=np.uint32)
        s = time.perf_counter()
        _ = np.sort(np_arr, kind='quicksort')
        t_np_quick = (time.perf_counter() - s) * 1000.0
        results["numpy.sort(kind='quicksort')"] = t_np_quick

        # 6. numpy.sort(mergesort)
        np_arr = np.array(raw_list, dtype=np.uint32)
        s = time.perf_counter()
        _ = np.sort(np_arr, kind='mergesort')
        t_np_merge = (time.perf_counter() - s) * 1000.0
        results["numpy.sort(kind='mergesort')"] = t_np_merge

        # 7. numpy.sort(heapsort)
        np_arr = np.array(raw_list, dtype=np.uint32)
        s = time.perf_counter()
        _ = np.sort(np_arr, kind='heapsort')
        t_np_heap = (time.perf_counter() - s) * 1000.0
        results["numpy.sort(kind='heapsort')"] = t_np_heap

        # 8. numpy.sort(stable)
        np_arr = np.array(raw_list, dtype=np.uint32)
        s = time.perf_counter()
        _ = np.sort(np_arr, kind='stable')
        t_np_stable = (time.perf_counter() - s) * 1000.0
        results["numpy.sort(kind='stable')"] = t_np_stable

        # 9. pandas.Series.sort_values()
        s_series = pd.Series(raw_list, dtype='uint32')
        s = time.perf_counter()
        _ = s_series.sort_values()
        t_pd = (time.perf_counter() - s) * 1000.0
        results["pandas.Series.sort_values()"] = t_pd

        # Print comparison table
        print(f"  {'Python Sorter Engine':<44} {'Time (ms)':<14} {'Speedup vs list.sort()'}")
        print("  " + "-" * 78)

        for engine_name, t_ms in results.items():
            speedup_vs_timsort = t_list / t_ms
            if "qi_sort" in engine_name:
                print(f"  {engine_name:<44} {t_ms:<14.2f} {speedup_vs_timsort:.2f}x FASTER vs Timsort")
            else:
                print(f"  {engine_name:<44} {t_ms:<14.2f} {speedup_vs_timsort:.2f}x speedup vs Timsort")
        print("\n")

if __name__ == "__main__":
    run_benchmark()
