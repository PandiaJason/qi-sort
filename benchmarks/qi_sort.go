package main

import (
	"fmt"
	"math/rand"
	"slices"
	"sort"
	"time"
)

// ── qiSort: Go 2-Pass Radix-16 Zero-Memcpy Engine ──
func qiSort(data []uint32) {
	n := len(data)
	if n <= 1 {
		return;
	}

	// Early O(N) pre-sorted check
	if n >= 64 {
		isSorted := true
		limit := 1024
		if n < limit {
			limit = n
		}
		for i := 1; i < limit; i++ {
			if data[i-1] > data[i] {
				isSorted = false
				break
			}
		}
		if isSorted && slices.IsSorted(data) {
			return
		}
	}

	buf := make([]uint32, n)
	var c0 [65536]uint32
	var c1 [65536]uint32

	// 1 combined count pass
	for i := 0; i < n; i++ {
		v := data[i]
		c0[v&0xFFFF]++
		c1[v>>16]++
	}

	skipPass1 := (c1[0] == uint32(n))

	var s0, s1 uint32
	for k := 0; k < 65536; k++ {
		t0 := c0[k]
		c0[k] = s0
		s0 += t0
		if !skipPass1 {
			t1 := c1[k]
			c1[k] = s1
			s1 += t1
		}
	}

	// Pass 0: data -> buf (bits 0-15)
	for i := 0; i < n; i++ {
		v := data[i]
		idx := v & 0xFFFF
		buf[c0[idx]] = v
		c0[idx]++
	}

	if skipPass1 {
		copy(data, buf)
		return
	}

	// Pass 1: buf -> data (bits 16-31) — Ends directly in output data! 0 MEMCPY!
	for i := 0; i < n; i++ {
		v := buf[i]
		idx := v >> 16
		data[c1[idx]] = v
		c1[idx]++
	}
}

func main() {
	const N = 2_000_000
	rng := rand.New(rand.NewSource(42))

	uniform := make([]uint32, N)
	dupes := make([]uint32, N)
	nearly := make([]uint32, N)

	for i := 0; i < N; i++ {
		uniform[i] = rng.Uint32()
		dupes[i] = uint32(rng.Intn(256))
	}
	copy(nearly, uniform)
	slices.Sort(nearly)
	for i := 0; i < N/20; i++ {
		nearly[rng.Intn(N)] = rng.Uint32()
	}

	runBench := func(name string, orig []uint32) {
		check := make([]uint32, len(orig))
		copy(check, orig)
		qiSort(check)
		ok := slices.IsSorted(check)

		minQi := 1e9
		minSlices := 1e9
		minSort := 1e9

		for r := 0; r < 5; r++ {
			// Benchmark qiSort
			d1 := make([]uint32, len(orig))
			copy(d1, orig)
			t0 := time.Now()
			qiSort(d1)
			dt := time.Since(t0).Seconds() * 1000.0
			if dt < minQi {
				minQi = dt
			}

			// Benchmark slices.Sort (Go 1.21+ pdqsort)
			d2 := make([]uint32, len(orig))
			copy(d2, orig)
			t0 = time.Now()
			slices.Sort(d2)
			dt = time.Since(t0).Seconds() * 1000.0
			if dt < minSlices {
				minSlices = dt
			}

			// Benchmark legacy sort.Slice
			d3 := make([]uint32, len(orig))
			copy(d3, orig)
			t0 = time.Now()
			sort.Slice(d3, func(i, j int) bool { return d3[i] < d3[j] })
			dt = time.Since(t0).Seconds() * 1000.0
			if dt < minSort {
				minSort = dt
			}
		}

		status := "PASS"
		if !ok {
			status = "FAIL"
		}

		fmt.Printf("=== %s (N = %d) === [%s]\n", name, N, status)
		fmt.Printf("  slices.Sort (pdqsort): %6.2f ms\n", minSlices)
		fmt.Printf("  sort.Slice (legacy):   %6.2f ms\n", minSort)
		fmt.Printf("  qiSort (radix):        %6.2f ms  (%.2fx speedup vs slices.Sort)\n\n", minQi, minSlices/minQi)
	}

	fmt.Println("========================================================================")
	fmt.Println("              GO (GOLANG) FULL SORTING BENCHMARK")
	fmt.Println("========================================================================")
	fmt.Println()

	runBench("Uniform Random (32-bit)", uniform)
	runBench("Heavy Duplicates (0-255)", dupes)
	runBench("Nearly Sorted (95%)", nearly)
}
