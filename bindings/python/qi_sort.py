"""
qi-sort: High-Performance Adaptive Sorting Suite
=================================================
Ultra-fast sorting for NumPy arrays (uint32, int32, float32, uint64, int64, float64) and Python lists.

Supported Models:
- 'apex' / 'default' : qi::apex ULTIMATE (Strict 20KB L1-bound, 8-way ILP, fastest on silicon)
- 'field'            : QI-FieldSort (100% Non-Radix Continuous Density-Field Inversion)
- 'wave'             : QI-WaveSort (Wavefunction Collapse with Block Cache)
- 'partition'        : QI Partition Sort (Fixed-Point Q32.32 Micro-Buckets)
- 'turbo'            : QI Turbo Radix (4-Banked Dual-Histogram)
- 'radix8', 'radix11', 'radix16' : Standard fixed-width radix passes

Usage:
    import qi_sort
    import numpy as np

    # 1. Default Flagship (qi::apex)
    data = np.random.randint(0, 2**32-1, size=1_000_000, dtype=np.uint32)
    qi_sort.sort(data)

    # 2. Select Any QI Model
    qi_sort.sort(data, model='field')     # 100% Non-radix
    qi_sort.sort(data, model='apex')      # Flagship apex

    # 3. Signed, Float, 64-bit and Parallel
    floats = np.random.randn(1_000_000).astype(np.float32)
    qi_sort.sort(floats)
    qi_sort.parallel_sort(data)

    # 4. Database Tuple Pairs (ORDER BY keys)
    keys = np.random.randint(0, 1000, size=100_000, dtype=np.uint32)
    row_ids = np.arange(100_000, dtype=np.uint64)
    qi_sort.sort_pairs(keys, row_ids)
"""

import ctypes
import os
import sys
from pathlib import Path
from typing import Optional, Union, Any

# ── Locate and load shared library (libqisort) ──
_lib = None

def _load_library():
    global _lib
    if _lib is not None:
        return _lib

    search_dirs = [
        Path(__file__).parent.resolve(),
        Path(__file__).parent.parent.parent.resolve(),
        Path(__file__).parent.parent.resolve(),
        Path(os.getcwd()),
    ]
    
    lib_names = ["libqisort.dylib", "libqisort.so", "qisort.dll", "qi_sort_cpp.cpython-312-darwin.so"]
    
    for d in search_dirs:
        for name in lib_names:
            p = d / name
            if p.exists():
                try:
                    _lib = ctypes.CDLL(str(p))
                    _setup_cdll_signatures(_lib)
                    return _lib
                except Exception:
                    pass
    return None

def _setup_cdll_signatures(lib):
    # Apex & Base
    lib.qi_apex_sort_u32.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    lib.qi_apex_parallel_sort_u32.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_uint]
    lib.qi_apex_sort_i32.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    lib.qi_apex_sort_f32.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    lib.qi_apex_sort_u64.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    lib.qi_apex_sort_i64.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    lib.qi_apex_sort_f64.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    
    # Models
    lib.qi_field_sort_u32.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    lib.qi_wave_sort_u32.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    lib.qi_partition_sort_u32.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    lib.qi_turbo_sort_u32.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    
    # Pairs
    lib.qi_sort_pairs_u32_u64.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]

_lib = _load_library()


def sort(data: Any, model: str = 'apex') -> Any:
    """
    Sort data in-place using the specified QI model.

    Parameters
    ----------
    data  : np.ndarray or list
        Input array or list to sort.
    model : str, default 'apex'
        Algorithm engine:
        - 'apex' (Flagship 314 MKeys/s)
        - 'field' (100% Non-Radix Density-Field Inversion)
        - 'wave' (Continuous Wavefunction Collapse)
        - 'partition' (Q32.32 Micro-Bucket Partitioning)
        - 'turbo' (4-Banked Dual-Histogram Radix-11)
        - 'radix8', 'radix11', 'radix16' (Fixed Radix Kernels)
    """
    try:
        import numpy as np
        if isinstance(data, np.ndarray):
            _sort_numpy(data, model)
            return data
    except ImportError:
        pass

    if isinstance(data, list):
        data.sort()
        return data

    raise TypeError(f"Unsupported data type: {type(data)}")


def _sort_numpy(data, model: str = 'apex') -> None:
    import numpy as np
    if not data.flags['C_CONTIGUOUS']:
        raise ValueError("Array must be C-contiguous. Use np.ascontiguousarray().")
    if not data.flags['WRITEABLE']:
        raise ValueError("Array must be writable.")

    lib = _load_library()
    if lib is None:
        data.sort()
        return

    ptr = data.ctypes.data
    n = data.size
    dt = data.dtype
    m = model.lower()

    if dt == np.uint32:
        if m in ('field', 'qi_field', 'field_sort'):
            lib.qi_field_sort_u32(ptr, n)
        elif m in ('wave', 'qi_wave', 'wave_sort'):
            lib.qi_wave_sort_u32(ptr, n)
        elif m in ('partition', 'qi_partition', 'part'):
            lib.qi_partition_sort_u32(ptr, n)
        elif m in ('turbo', 'qi_turbo', 'turbo_radix'):
            lib.qi_turbo_sort_u32(ptr, n)
        elif m == 'radix8':
            lib.qi_radix8_u32(ptr, n, 1)
        elif m == 'radix11':
            lib.qi_radix11_u32(ptr, n, 1)
        elif m == 'radix16':
            lib.qi_radix16_u32(ptr, n, 1)
        else:
            lib.qi_apex_sort_u32(ptr, n)
    elif dt == np.int32:
        lib.qi_apex_sort_i32(ptr, n)
    elif dt == np.float32:
        lib.qi_apex_sort_f32(ptr, n)
    elif dt == np.uint64:
        lib.qi_apex_sort_u64(ptr, n)
    elif dt == np.int64:
        lib.qi_apex_sort_i64(ptr, n)
    elif dt == np.float64:
        lib.qi_apex_sort_f64(ptr, n)
    else:
        data.sort()


def parallel_sort(data: Any, num_threads: int = 0) -> Any:
    """Multi-core lock-free parallel sort."""
    try:
        import numpy as np
        if isinstance(data, np.ndarray) and data.dtype == np.uint32:
            lib = _load_library()
            if lib is not None:
                lib.qi_apex_parallel_sort_u32(data.ctypes.data, data.size, num_threads)
                return data
    except ImportError:
        pass
    return sort(data)


def sort_pairs(keys, payloads) -> None:
    """Sort database tuple pairs (keys and payloads) in-place by key."""
    import numpy as np
    if not (isinstance(keys, np.ndarray) and isinstance(payloads, np.ndarray)):
        raise TypeError("Both keys and payloads must be numpy arrays.")
    if keys.dtype != np.uint32 or payloads.dtype != np.uint64:
        raise TypeError("keys must be uint32 and payloads must be uint64.")
    if keys.size != payloads.size:
        raise ValueError("keys and payloads must have identical lengths.")

    lib = _load_library()
    if lib is not None:
        lib.qi_sort_pairs_u32_u64(keys.ctypes.data, payloads.ctypes.data, keys.size)
    else:
        order = np.argsort(keys)
        keys[:] = keys[order]
        payloads[:] = payloads[order]
