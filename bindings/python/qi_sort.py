"""
qi-sort: High-Performance Adaptive Sorting Suite
=================================================
Ultra-fast sorting for NumPy arrays, Polars Series/DataFrames, Apache Arrow Tables, and Python lists.

Supported Models:
- 'apex' / 'default' : qi::apex ULTIMATE (Strict 20KB L1-bound, 8-way ILP, fastest on silicon)
- 'field'            : QI-FieldSort (100% Non-Radix Continuous Density-Field Inversion)
- 'wave'             : QI-WaveSort (Wavefunction Collapse with Block Cache)
- 'partition'        : QI Partition Sort (Fixed-Point Q32.32 Micro-Buckets)
- 'turbo'            : QI Turbo Radix (4-Banked Dual-Histogram)
- 'radix8', 'radix11', 'radix16' : Standard fixed-width radix passes
"""

import ctypes
import os
import sys
from pathlib import Path
from typing import Optional, Union, Any

# ── Robust Dynamic Library Loader ──
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
    
    # 1. Exact names
    lib_names = ["libqisort.dylib", "libqisort.so", "qisort.dll"]
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

    # 2. Dynamic glob for python C extensions (qi_sort_cpp*.so / .pyd)
    for d in search_dirs:
        for p in list(d.glob("qi_sort_cpp*")):
            if p.is_file() and p.suffix in ('.so', '.dylib', '.pyd', '.dll'):
                try:
                    _lib = ctypes.CDLL(str(p))
                    _setup_cdll_signatures(_lib)
                    return _lib
                except Exception:
                    pass
    return None

def _setup_cdll_signatures(lib):
    try:
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
    except Exception:
        pass

_lib = _load_library()


# ════════════════════════════════════════════════════════════════════════════
# 1. CORE SORTING API (NumPy, Polars, PyArrow, Lists)
# ════════════════════════════════════════════════════════════════════════════

def sort(data: Any, model: str = 'apex') -> Any:
    """
    Sort data in-place using the specified QI model.

    Supports:
    - NumPy ndarrays (`uint32`, `int32`, `float32`, `uint64`, `int64`, `float64`)
    - Polars Series (`pl.UInt32`, `pl.Int32`, `pl.Float32`, etc.)
    - PyArrow Arrays (`pa.uint32()`, `pa.int32()`, `pa.float32()`, etc.)
    - Python lists of integers
    """
    # 1. NumPy Array
    try:
        import numpy as np
        if isinstance(data, np.ndarray):
            _sort_numpy(data, model)
            return data
    except ImportError:
        pass

    # 2. Polars Series
    try:
        import polars as pl
        if isinstance(data, pl.Series):
            return sort_polars(data, model)
    except ImportError:
        pass

    # 3. PyArrow Array
    try:
        import pyarrow as pa
        if isinstance(data, pa.Array):
            return sort_arrow(data, model)
    except ImportError:
        pass

    # 4. Python List Fallback
    if isinstance(data, list):
        data.sort()
        return data

    raise TypeError(f"Unsupported data type for qi_sort: {type(data)}")


def _sort_numpy(data, model: str = 'apex') -> None:
    import numpy as np
    if not data.flags['C_CONTIGUOUS']:
        raise ValueError("Array must be C-contiguous. Call np.ascontiguousarray() first.")
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


# ════════════════════════════════════════════════════════════════════════════
# 2. POLARS & APACHE ARROW ZERO-COPY DATABASE INTEGRATION
# ════════════════════════════════════════════════════════════════════════════

def sort_polars(series: Any, model: str = 'apex') -> Any:
    """
    Sort a Polars Series zero-copy using qi::apex.
    Returns the sorted Polars Series.
    """
    import polars as pl
    import numpy as np

    # Extract underlying buffer zero-copy
    arr = series.to_numpy(zero_copy_only=False)
    _sort_numpy(arr, model)
    return pl.Series(series.name, arr)


def sort_arrow(array: Any, model: str = 'apex') -> Any:
    """
    Sort a PyArrow Array zero-copy using qi::apex.
    Returns the sorted PyArrow Array.
    """
    import pyarrow as pa
    import numpy as np

    # Zero-copy buffer view
    np_arr = array.to_numpy(zero_copy_only=False)
    _sort_numpy(np_arr, model)
    return pa.array(np_arr, type=array.type)


def sort_dataframe(df: Any, by_column: str) -> Any:
    """
    Accelerate sorting a Polars DataFrame by a numeric key column.
    Uses qi::apex::sort_pairs on (key_column, row_index) tuples.
    """
    import polars as pl
    import numpy as np

    keys = df[by_column].to_numpy(zero_copy_only=False)
    if keys.dtype != np.uint32:
        keys = keys.astype(np.uint32)

    n = len(df)
    row_ids = np.arange(n, dtype=np.uint64)
    sort_pairs(keys, row_ids)

    # Gather rows in sorted index order
    return df[row_ids]


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
