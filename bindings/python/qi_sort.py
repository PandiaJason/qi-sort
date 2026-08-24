"""
QI-Sort Python Bindings (ctypes / NumPy)
Ultra-fast quantum-inspired adaptive radix sort for Python lists & NumPy arrays.
"""

import ctypes
import os
import platform
import time
from typing import Union, List, Tuple

# Load Native C Shared Library or C-Extension
def _load_library():
    dir_path = os.path.dirname(os.path.abspath(__file__))
    
    # 1. Look for compiled extension binary in the same directory as qi_sort.py
    if os.path.exists(dir_path):
        for f in os.listdir(dir_path):
            if f.startswith("qi_sort_cpp") and (f.endswith(".so") or f.endswith(".pyd") or f.endswith(".dylib")):
                try:
                    return ctypes.CDLL(os.path.join(dir_path, f))
                except Exception:
                    pass

    # 2. Attempt direct Python import of qi_sort_cpp
    try:
        import qi_sort_cpp
        return ctypes.CDLL(qi_sort_cpp.__file__)
    except Exception:
        pass

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
        os.path.join(project_root, "src", lib_name),
        os.path.join(project_root, lib_name),
        os.path.join(dir_path, lib_name),
        os.path.join("/usr/local/lib", lib_name),
    ]

    for path in search_paths:
        if os.path.exists(path):
            return ctypes.CDLL(path)
            
    raise FileNotFoundError(f"Could not find native {lib_name} or qi_sort_cpp extension. Please run 'pip install .' or build libqisort.")

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


import array

def sort(data: Union[List[int], any]) -> Union[List[int], any]:
    """
    Sort a Python list of non-negative 32-bit integers, array.array, or NumPy uint32 array in-place.
    
    Example:
        import qi_sort
        data = [10, 5, 20, 1]
        qi_sort.sort(data)
    """
    # 1. NumPy Array (Zero-Copy C Pointer)
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

    # 2. Python array.array('I') (Zero-Copy C Pointer)
    if isinstance(data, array.array):
        if data.typecode != 'I':
            raise TypeError("array.array must be of typecode 'I' (unsigned 32-bit int)")
        c_ptr = (ctypes.c_uint32 * len(data)).from_buffer(data)
        _lib.qi_sort_u32(c_ptr, len(data))
        return data

    # 3. Standard Python list handling (Optimized via C array buffer)
    if not isinstance(data, list):
        raise TypeError("Input must be a list of integers, array.array('I'), or a uint32 NumPy array.")

    n = len(data)
    if n <= 1:
        return data

    # Fast conversion to C array via array.array buffer
    arr_buf = array.array('I', data)
    c_ptr = (ctypes.c_uint32 * n).from_buffer(arr_buf)
    _lib.qi_sort_u32(c_ptr, n)
    
    # Update Python list in-place
    data[:] = arr_buf.tolist()
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
