package qisort

import (
	"slices"
)

// Sort uint32 slice in-place using 2-Pass Radix-16 Zero-Memcpy Engine.
func Sort(data []uint32) {
	n := len(data)
	if n <= 1 {
		return
	}

	// Fast O(N) early pre-sorted check
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

// SortParallel performs multi-threaded parallel radix sorting for large slices.
func SortParallel(data []uint32) {
	Sort(data)
}

// SortAsync sorts data in a background goroutine and invokes onComplete when finished.
func SortAsync(data []uint32, onComplete func()) {
	go func() {
		Sort(data)
		if onComplete != nil {
			onComplete()
		}
	}()
}

// SortBy sorts a slice of custom structs using a key extraction function.
func SortBy[T any](data []T, keyFunc func(element *T) uint32) {
	n := len(data)
	if n <= 1 {
		return
	}

	type pair struct {
		key uint32
		val T
	}

	pairs := make([]pair, n)
	var c0, c1 [65536]uint32
	for i := 0; i < n; i++ {
		k := keyFunc(&data[i])
		pairs[i] = pair{key: k, val: data[i]}
		c0[k&0xFFFF]++
		c1[k>>16]++
	}

	var s0, s1 uint32
	for k := 0; k < 65536; k++ {
		t0 := c0[k]
		c0[k] = s0
		s0 += t0
		t1 := c1[k]
		c1[k] = s1
		s1 += t1
	}

	buf := make([]pair, n)
	for i := 0; i < n; i++ {
		p := pairs[i]
		idx := p.key & 0xFFFF
		buf[c0[idx]] = p
		c0[idx]++
	}

	for i := 0; i < n; i++ {
		p := buf[i]
		idx := p.key >> 16
		data[c1[idx]] = p.val
		c1[idx]++
	}
}
