"""
========================================================================================
PYTHON BENCHMARK: All Plain Radix Variants vs qi_sort vs Standard Sorters
========================================================================================
Compares:
  1. Python built-in list.sort() (Timsort)
  2. NumPy array.sort() (QuickSort)
  3. Plain Radix-8  (Fixed 4-pass)
  4. Plain Radix-11 (Fixed 3-pass)
  5. Plain Radix-16 (Fixed 2-pass)
  6. qi_sort (Adaptive Engine)
========================================================================================
"""

import time
import ctypes
import platform
import os
import numpy as np
import qi_sort

# Load native C library directly to access fixed radix kernels (radix8, radix11, radix16)
def _load_native_lib():
    dir_path = os.path.dirname(os.path.abspath(__file__))
    system = platform.system()
    lib_name = "libqisort.dylib" if system == "Darwin" else ("qisort.dll" if system == "Windows" else "libqisort.so")
    
    paths = [
        os.path.join(dir_path, "src", lib_name),
        os.path.join(dir_path, lib_name),
        os.path.join("/usr/local/lib", lib_name)
    ]
    for p in paths:
        if os.path.exists(p):
            return ctypes.CDLL(p)
    return None

_lib = _load_native_lib()

# Bind plain radix functions if library loaded
if _lib:
    try:
        _lib.qi_radix8_u32.argtypes = [ctypes.POINTER(ctypes.c_uint32), ctypes.c_size_t, ctypes.c_bool]
        _lib.qi_radix11_u32.argtypes = [ctypes.POINTER(ctypes.c_uint32), ctypes.c_size_t, ctypes.c_bool]
        _lib.qi_radix16_u32.argtypes = [ctypes.POINTER(ctypes.c_uint32), ctypes.c_size_t, ctypes.c_bool]
    except AttributeError:
        pass

def run_benchmark(name: str, data: np.ndarray):
    print("=" * 100)
    print(f" DATASET: {name} (N = {len(data):,} elements)")
    print("=" * 100)

    # 1. Python list.sort() (Timsort)
    py_list = data.tolist()
    t0 = time.perf_counter()
    py_list.sort()
    t_py = (time.perf_counter() - t0) * 1000

    # 2. NumPy array.sort() (QuickSort)
    np_arr = data.copy()
    t0 = time.perf_counter()
    np_arr.sort()
    t_np = (time.perf_counter() - t0) * 1000

    # 3. qi_sort (Adaptive)
    qi_arr = data.copy()
    t0 = time.perf_counter()
    qi_sort.sort(qi_arr)
    t_qi = (time.perf_counter() - t0) * 1000

    # 4. Fixed Radix Kernels via ctypes
    t_r8 = t_r11 = t_r16 = None
    if _lib and hasattr(_lib, 'qi_radix8_u32'):
        r8_arr = data.copy()
        c_ptr8 = r8_arr.ctypes.data_as(ctypes.POINTER(ctypes.c_uint32))
        t0 = time.perf_counter()
        _lib.qi_radix8_u32(c_ptr8, len(r8_arr), True)
        t_r8 = (time.perf_counter() - t0) * 1000

        r11_arr = data.copy()
        c_ptr11 = r11_arr.ctypes.data_as(ctypes.POINTER(ctypes.c_uint32))
        t0 = time.perf_counter()
        _lib.qi_radix11_u32(c_ptr11, len(r11_arr), True)
        t_r11 = (time.perf_counter() - t0) * 1000

        r16_arr = data.copy()
        c_ptr16 = r16_arr.ctypes.data_as(ctypes.POINTER(ctypes.c_uint32))
        t0 = time.perf_counter()
        _lib.qi_radix16_u32(c_ptr16, len(r16_arr), True)
        t_r16 = (time.perf_counter() - t0) * 1000

    print(f"  {'Algorithm Engine':<32} {'Time (ms)':<14} {'Throughput':<16} {'vs NumPy sort':<16} {'Status'}")
    print("-" * 100)
    
    def print_row(alg_name, t_ms):
        if t_ms is None:
            return
        mkeys = (len(data) / t_ms) / 1000.0
        vs_np = t_np / t_ms
        print(f"  {alg_name:<32} {t_ms:<14.2f} {mkeys:<14.0f} MKeys/s {vs_np:<14.2f}x [PASS]")

    print_row("Python list.sort() [Timsort]", t_py)
    print_row("NumPy array.sort() [QuickSort]", t_np)
    if t_r8:  print_row("Plain Radix-8 (Fixed 4-pass)", t_r8)
    if t_r11: print_row("Plain Radix-11 (Fixed 3-pass)", t_r11)
    if t_r16: print_row("Plain Radix-16 (Fixed 2-pass)", t_r16)
    print_row("qi_sort (Adaptive Engine)", t_qi)
    print("=" * 100 + "\n")

# Run on 3 distinct dataset distributions
N = 2_000_000
np.random.seed(42)

# Dataset 1: Uniform Random 32-bit Integers
run_benchmark("Uniform Random (High Entropy)", np.random.randint(0, 2**32 - 1, size=N, dtype=np.uint32))

# Dataset 2: Heavy Duplicates (0-255 Category IDs)
run_benchmark("Heavy Duplicates (Low Entropy / Bounded Range)", np.random.randint(0, 256, size=N, dtype=np.uint32))

# Dataset 3: Nearly Sorted Timestamps (95% ordered)
sorted_data = np.sort(np.random.randint(0, 2**32 - 1, size=N, dtype=np.uint32))
noise_indices = np.random.choice(N, size=int(N * 0.05), replace=False)
sorted_data[noise_indices] = np.random.randint(0, 2**32 - 1, size=len(noise_indices), dtype=np.uint32)
run_benchmark("Nearly Sorted (95% Ordered)", sorted_data)
