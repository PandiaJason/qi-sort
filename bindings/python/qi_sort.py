"""
qi-sort: Quantum-Inspired Adaptive Radix Sort
==============================================
Ultra-fast sorting for NumPy uint32 arrays and Python lists.

Usage:
    import qi_sort
    import numpy as np

    data = np.random.randint(0, 2**32-1, size=2_000_000, dtype=np.uint32)
    qi_sort.sort(data)          # in-place, beats NumPy on all distributions
    qi_sort.sort(data, alg=8)   # force Radix-8
    qi_sort.sort(data, alg=11)  # force Radix-11
    qi_sort.sort(data, alg=16)  # force Radix-16

PyPI: https://pypi.org/project/qi-sort/
GitHub: https://github.com/PandiaJason/qi-sort
"""

from typing import List, Optional, Union

# ── Load native C++ extension ───────────────────────────────────────────────
try:
    import qi_sort_cpp as _cpp
    _HAS_CPP = True
except ImportError:
    _cpp = None  # type: ignore
    _HAS_CPP = False


def _numpy_sort(data, alg: Optional[int]) -> None:
    """Sort a NumPy uint32 array in-place using the native C++ engine."""
    import numpy as np

    if not isinstance(data, np.ndarray):
        raise TypeError("Expected a numpy ndarray")
    if data.dtype != np.uint32:
        raise TypeError(f"Expected dtype=np.uint32, got {data.dtype}")
    if not data.flags['C_CONTIGUOUS']:
        raise TypeError("Array must be C-contiguous. Call np.ascontiguousarray() first.")
    if not data.flags['WRITEABLE']:
        raise TypeError("Array must be writable.")

    ptr = data.ctypes.data          # raw int pointer — always works on all platforms/numpy versions
    n   = data.size

    if not _HAS_CPP:
        raise ImportError(
            "qi_sort native extension (qi_sort_cpp) not found. "
            "Install with: pip install qi-sort"
        )

    if alg == 8:
        _cpp.radix8_ptr(ptr, n)
    elif alg == 11:
        _cpp.radix11_ptr(ptr, n)
    elif alg == 16:
        _cpp.radix16_ptr(ptr, n)
    else:
        _cpp.sort_ptr(ptr, n)   # IPR-guided adaptive engine


def sort(data, alg: Optional[int] = None):
    """
    Sort data in-place using the qi adaptive radix engine.

    Parameters
    ----------
    data : np.ndarray (uint32) or list[int]
        Input to sort. NumPy uint32 arrays are sorted zero-copy in C++.
    alg  : int, optional
        Force a specific radix kernel: 8, 11, or 16.
        Default (None) uses the IPR-guided adaptive engine.

    Returns
    -------
    data (same object, sorted in-place)

    Examples
    --------
    >>> import numpy as np, qi_sort
    >>> a = np.random.randint(0, 2**32-1, size=2_000_000, dtype=np.uint32)
    >>> qi_sort.sort(a)
    """
    try:
        import numpy as np
        if isinstance(data, np.ndarray):
            _numpy_sort(data, alg)
            return data
    except ImportError:
        pass

    # Python list fallback
    if isinstance(data, list):
        if _HAS_CPP:
            _cpp.sort(data)   # C++ vector sort for Python lists
        else:
            data.sort()       # pure Python Timsort last resort
        return data

    raise TypeError(
        f"sort() expects a numpy uint32 array or Python list, got {type(data).__name__}"
    )


def radix8(data) -> None:
    """Force Radix-8 (4-pass, 8-bit buckets). Good for narrow distributions."""
    return sort(data, alg=8)


def radix11(data) -> None:
    """Force Radix-11 (3-pass, 11-bit buckets). Best for uniform 32-bit data."""
    return sort(data, alg=11)


def radix16(data) -> None:
    """Force Radix-16 (2-pass, 16-bit buckets). Best for heavily skewed data."""
    return sort(data, alg=16)


def analyze(data) -> dict:
    """
    Analyse a uint32 array and return IPR sensing metrics used by qi::sort.

    Returns
    -------
    dict with keys: entropy, ipr, effective_states, duplicate_ratio
    """
    import numpy as np
    import ctypes

    if not isinstance(data, np.ndarray) or data.dtype != np.uint32:
        raise TypeError("analyze() requires a numpy uint32 array")

    sample = np.ascontiguousarray(data[:1024], dtype=np.uint32)
    N = len(sample)
    counts = np.bincount(sample, minlength=256)[:256]
    p = counts / N
    nonzero = p[p > 0]
    ipr = float(np.sum(nonzero ** 2))
    eff = 1.0 / ipr if ipr > 0 else 256.0
    entropy = float(-np.sum(nonzero * np.log2(nonzero))) / 8.0
    dup_ratio = 1.0 - len(nonzero) / 256.0

    return {
        "entropy":          round(entropy,     6),
        "ipr":              round(ipr,          6),
        "effective_states": round(eff,          4),
        "duplicate_ratio":  round(dup_ratio,    6),
    }
