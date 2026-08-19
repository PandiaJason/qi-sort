"""
QI-Sort Python Bindings (ctypes / NumPy)
Ultra-fast quantum-inspired adaptive radix sort for Python lists & NumPy arrays.
"""

import ctypes
import os
import platform
import time
from typing import Union, List, Tuple

# Load Native C Shared Library
def _load_library():
    dir_path = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(dir_path, "../../"))
    
    system = platform.system()
    if system == "Darwin":
        lib_name = "libqisort.dylib"
    elif system == "Windows":
        lib_name = "qisort.dll"
    else:
        lib_name = "libqisort.so"

    search_paths = [
        os.path.join(project_root, lib_name),
        os.path.join(dir_path, lib_name),
        os.path.join("/usr/local/lib", lib_name),
    ]

    for path in search_paths:
        if os.path.exists(path):
            return ctypes.CDLL(path)
            
    raise FileNotFoundError(f"Could not find native {lib_name}. Please build libqisort first.")

_lib = _load_library()

# Function Signatures
_lib.qi_sort_u32.argtypes = [ctypes.POINTER(ctypes.c_uint32), ctypes.c_size_t]
_lib.qi_sort_u32.restype = None

_lib.qi_analyze_u32.argtypes = [
    ctypes.POINTER(ctypes.c_uint32),
    ctypes.c_size_t,
    ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double)
]
_lib.qi_analyze_u32.restype = None


def sort(data: Union[List[int], any]) -> Union[List[int], any]:
    """
    Sort a Python list of non-negative 32-bit integers or NumPy uint32 array in-place.
    
    Example:
        import qi_sort
        data = [10, 5, 20, 1]
        qi_sort.sort(data)
    """
    # Check if NumPy array
    try:
        import numpy as np
        if isinstance(data, np.ndarray):
            if data.dtype != np.uint32:
                data = data.astype(np.uint32)
            c_ptr = data.ctypes.data_as(ctypes.POINTER(ctypes.c_uint32))
            _lib.qi_sort_u32(c_ptr, data.size)
            return data
    except ImportError:
        pass

    # Standard Python list handling
    if not isinstance(data, list):
        raise TypeError("Input must be a list of integers or a uint32 NumPy array.")

    n = len(data)
    if n <= 1:
        return data

    c_array = (ctypes.c_uint32 * n)(*data)
    _lib.qi_sort_u32(c_array, n)
    
    for i in range(n):
        data[i] = c_array[i]

    return data


def analyze(data: Union[List[int], any]) -> dict:
    """
    Perform non-destructive statistical state vector analysis on data.
    Returns entropy, IPR, effective occupied buckets (N_eff), and duplicate ratio.
    """
    try:
        import numpy as np
        if isinstance(data, np.ndarray):
            if data.dtype != np.uint32:
                data = data.astype(np.uint32)
            c_ptr = data.ctypes.data_as(ctypes.POINTER(ctypes.c_uint32))
            n = data.size
        else:
            n = len(data)
            c_ptr = (ctypes.c_uint32 * n)(*data)
    except ImportError:
        n = len(data)
        c_ptr = (ctypes.c_uint32 * n)(*data)

    entropy = ctypes.c_double()
    ipr = ctypes.c_double()
    neff = ctypes.c_double()
    dup_ratio = ctypes.c_double()

    _lib.qi_analyze_u32(
        c_ptr, n,
        ctypes.byref(entropy),
        ctypes.byref(ipr),
        ctypes.byref(neff),
        ctypes.byref(dup_ratio)
    )

    return {
        "entropy": entropy.value,
        "ipr": ipr.value,
        "effective_states": neff.value,
        "duplicate_ratio": dup_ratio.value
    }
