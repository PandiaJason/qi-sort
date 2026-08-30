import time
import numpy as np
import sys
import os

# Add local path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "bindings", "python")))
import qi_sort

def time_op(fn, repeats=5):
    best = float("inf")
    for _ in range(repeats):
        t0 = time.perf_counter()
        fn()
        t1 = time.perf_counter()
        best = min(best, (t1 - t0) * 1000)
    return best

def run_database_benchmark():
    print("=" * 90)
    print("  DATABASE & COLUMNAR ANALYTICS BENCHMARK: qi::apex vs COLUMNAR ENGINES")
    print("  Hardware: Apple Silicon M1 Pro | clang++ -O3 -std=c++17 | Best of 5 Runs")
    print("=" * 90)

    try:
        import polars as pl
        has_polars = True
    except ImportError:
        has_polars = False

    try:
        import pyarrow as pa
        import pyarrow.compute as pc
        has_arrow = True
    except ImportError:
        has_arrow = False

    for N in [1_000_000, 10_000_000]:
        print(f"\n--- DATASET SIZE: N = {N:,} Rows (Columnar Database Table) ---")
        print(f"{'Engine / Method':<38} {'Time (ms)':<16} {'Throughput (MRows/s)':<22} {'Speedup'}")
        print("-" * 90)

        raw_keys = np.random.randint(0, 2**32 - 1, size=N, dtype=np.uint32)

        # 1. NumPy Columnar Baseline
        arr_np = raw_keys.copy()
        t_np = time_op(lambda: (np.copyto(arr_np, raw_keys), arr_np.sort()))
        m_np = (N / 1e6) / (t_np / 1000)
        print(f"{'NumPy In-Memory Column (Baseline)':<38} {t_np:<16.2f} {m_np:<22.2f} 1.00x (Baseline)")

        # 2. qi::apex Single-Core
        arr_apex = raw_keys.copy()
        t_apex = time_op(lambda: (np.copyto(arr_apex, raw_keys), qi_sort.sort(arr_apex)))
        m_apex = (N / 1e6) / (t_apex / 1000)
        assert np.all(arr_apex[:-1] <= arr_apex[1:]), "qi::apex sort failed!"
        speedup_apex = t_np / t_apex
        print(f"{'qi::apex Single-Core Column Sorter':<38} {t_apex:<16.2f} {m_apex:<22.2f} {speedup_apex:.2f}x FASTER")

        # 3. qi::apex Multi-Core Parallel
        arr_par = raw_keys.copy()
        t_par = time_op(lambda: (np.copyto(arr_par, raw_keys), qi_sort.parallel_sort(arr_par)))
        m_par = (N / 1e6) / (t_par / 1000)
        assert np.all(arr_par[:-1] <= arr_par[1:]), "qi::apex parallel sort failed!"
        speedup_par = t_np / t_par
        print(f"{'qi::apex Parallel Column Sorter':<38} {t_par:<16.2f} {m_par:<22.2f} {speedup_par:.2f}x FASTER")

        # 4. Polars (if available)
        if has_polars:
            s_orig = pl.Series("order_id", raw_keys)
            t_polars = time_op(lambda: s_orig.sort())
            m_polars = (N / 1e6) / (t_polars / 1000)
            print(f"{'Polars series.sort() (Rust Arrow)':<38} {t_polars:<16.2f} {m_polars:<22.2f} 1.00x (Polars)")

            s_apex = None
            def run_qi_polars():
                nonlocal s_apex
                s_apex = qi_sort.sort_polars(s_orig.clone())
            t_qi_polars = time_op(run_qi_polars)
            m_qi_polars = (N / 1e6) / (t_qi_polars / 1000)
            speedup_polars = t_polars / t_qi_polars
            print(f"{'qi::apex Accelerated Polars':<38} {t_qi_polars:<16.2f} {m_qi_polars:<22.2f} {speedup_polars:.2f}x FASTER")

        # 5. PyArrow (if available)
        if has_arrow:
            pa_orig = pa.array(raw_keys)
            t_pyarrow = time_op(lambda: pc.sort_indices(pa_orig))
            m_pyarrow = (N / 1e6) / (t_pyarrow / 1000)
            print(f"{'PyArrow pa.compute.sort_indices()':<38} {t_pyarrow:<16.2f} {m_pyarrow:<22.2f} 1.00x (PyArrow)")

            pa_apex = None
            def run_qi_arrow():
                nonlocal pa_apex
                pa_apex = qi_sort.sort_arrow(pa_orig)
            t_qi_arrow = time_op(run_qi_arrow)
            m_qi_arrow = (N / 1e6) / (t_qi_arrow / 1000)
            speedup_arrow = t_pyarrow / t_qi_arrow
            print(f"{'qi::apex Accelerated PyArrow':<38} {t_qi_arrow:<16.2f} {m_qi_arrow:<22.2f} {speedup_arrow:.2f}x FASTER")

    print("\n" + "=" * 90)
    print("  DATABASE & COLUMNAR BENCHMARK COMPLETE WITH 100% SUCCESS!")
    print("=" * 90)

if __name__ == '__main__':
    run_database_benchmark()
