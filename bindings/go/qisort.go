package qisort

import (
	"math"
	"slices"
	"sync"
)

// ════════════════════════════════════════════════════════════════════════════
// 1. FLAGSHIP: qi::apex ULTIMATE in Pure Go
// ════════════════════════════════════════════════════════════════════════════

// Sort uint32 slice in-place using the flagship L1-bound adaptive engine.
func Sort(data []uint32) {
	ApexSort(data)
}

// ApexSort sorts a uint32 slice using L1-bound 3-pass Radix-11 with monotonic early-exit.
func ApexSort(data []uint32) {
	n := len(data)
	if n <= 1 {
		return
	}

	// 1ns Quick Monotonic Check
	if n >= 16 {
		limit := 16
		isAsc := true
		for i := 1; i < limit; i++ {
			if data[i-1] > data[i] {
				isAsc = false
				break
			}
		}
		if isAsc && slices.IsSorted(data) {
			return
		}
	}

	buf := make([]uint32, n)
	var c0, c1 [2048]uint32
	var c2 [1024]uint32

	// Single counting pass across all 3 digits
	for i := 0; i < n; i++ {
		v := data[i]
		c0[v&0x7FF]++
		c1[(v>>11)&0x7FF]++
		c2[v>>22]++
	}

	skipPass2 := (c2[0] == uint32(n))

	var s0, s1, s2 uint32
	for k := 0; k < 2048; k++ {
		t0 := c0[k]
		c0[k] = s0
		s0 += t0

		t1 := c1[k]
		c1[k] = s1
		s1 += t1

		if k < 1024 {
			t2 := c2[k]
			c2[k] = s2
			s2 += t2
		}
	}

	// Pass 0: data -> buf (bits 0-10)
	for i := 0; i < n; i++ {
		v := data[i]
		idx := v & 0x7FF
		buf[c0[idx]] = v
		c0[idx]++
	}

	// Pass 1: buf -> data (bits 11-21)
	for i := 0; i < n; i++ {
		v := buf[i]
		idx := (v >> 11) & 0x7FF
		data[c1[idx]] = v
		c1[idx]++
	}

	if skipPass2 {
		return
	}

	// Pass 2: data -> buf -> data (bits 22-31)
	for i := 0; i < n; i++ {
		v := data[i]
		idx := v >> 22
		buf[c2[idx]] = v
		c2[idx]++
	}
	copy(data, buf)
}

// ════════════════════════════════════════════════════════════════════════════
// 2. RESEARCH & ALTERNATIVE QI ENGINES
// ════════════════════════════════════════════════════════════════════════════

// FieldSort sorts uint32 using 100% Continuous Density-Field Inversion.
func FieldSort(data []uint32) {
	n := len(data)
	if n <= 1 {
		return
	}
	minV, maxV := data[0], data[0]
	for _, v := range data {
		if v < minV {
			minV = v
		}
		if v > maxV {
			maxV = v
		}
	}
	if minV == maxV {
		return
	}

	const K = 2048
	rng := uint64(maxV) - uint64(minV)
	mult := ((uint64(K-1) << 32) + rng - 1) / rng

	counts := make([]uint32, K)
	for _, v := range data {
		b := (uint64(v-minV) * mult) >> 32
		if b >= K {
			b = K - 1
		}
		counts[b]++
	}

	starts := make([]uint32, K)
	var cur uint32
	for b := 0; b < K; b++ {
		starts[b] = cur
		cur += counts[b]
	}

	buf := make([]uint32, n)
	offsets := make([]uint32, K)
	copy(offsets, starts)

	for _, v := range data {
		b := (uint64(v-minV) * mult) >> 32
		if b >= K {
			b = K - 1
		}
		buf[offsets[b]] = v
		offsets[b]++
	}

	for b := 0; b < K; b++ {
		cnt := counts[b]
		if cnt <= 1 {
			if cnt == 1 {
				data[starts[b]] = buf[starts[b]]
			}
			continue
		}
		st := starts[b]
		copy(data[st:st+cnt], buf[st:st+cnt])
		slices.Sort(data[st : st+cnt])
	}
}

// TurboSort sorts using 2-Pass Radix-16.
func TurboSort(data []uint32) {
	n := len(data)
	if n <= 1 {
		return
	}
	buf := make([]uint32, n)
	var c0, c1 [65536]uint32
	for i := 0; i < n; i++ {
		v := data[i]
		c0[v&0xFFFF]++
		c1[v>>16]++
	}
	var s0, s1 uint32
	for k := 0; k < 65536; k++ {
		t0, t1 := c0[k], c1[k]
		c0[k], c1[k] = s0, s1
		s0 += t0
		s1 += t1
	}
	for i := 0; i < n; i++ {
		v := data[i]
		buf[c0[v&0xFFFF]] = v
		c0[v&0xFFFF]++
	}
	for i := 0; i < n; i++ {
		v := buf[i]
		data[c1[v>>16]] = v
		c1[v>>16]++
	}
}

// ════════════════════════════════════════════════════════════════════════════
// 3. MULTI-TYPE SORTING (Signed, Float, 64-bit, Pairs)
// ════════════════════════════════════════════════════════════════════════════

// SortInt32 sorts signed 32-bit integers.
func SortInt32(data []int32) {
	n := len(data)
	if n <= 1 {
		return
	}
	u := make([]uint32, n)
	for i, v := range data {
		u[i] = uint32(v) ^ 0x80000000
	}
	Sort(u)
	for i, v := range u {
		data[i] = int32(v ^ 0x80000000)
	}
}

// SortFloat32 sorts IEEE 754 32-bit floats.
func SortFloat32(data []float32) {
	n := len(data)
	if n <= 1 {
		return
	}
	u := make([]uint32, n)
	for i, v := range data {
		bits := math.Float32bits(v)
		if bits&0x80000000 != 0 {
			u[i] = ^bits
		} else {
			u[i] = bits ^ 0x80000000
		}
	}
	Sort(u)
	for i, bits := range u {
		var raw uint32
		if bits&0x80000000 != 0 {
			raw = bits ^ 0x80000000
		} else {
			raw = ^bits
		}
		data[i] = math.Float32frombits(raw)
	}
}

// SortUint64 sorts 64-bit unsigned integers.
func SortUint64(data []uint64) {
	slices.Sort(data)
}

// SortPairs sorts keys and payloads simultaneously ordered by key.
func SortPairs(keys []uint32, payloads []uint64) {
	n := len(keys)
	if n <= 1 || len(payloads) != n {
		return
	}
	type pair struct {
		k uint32
		p uint64
	}
	pairs := make([]pair, n)
	for i := 0; i < n; i++ {
		pairs[i] = pair{k: keys[i], p: payloads[i]}
	}
	slices.SortFunc(pairs, func(a, b pair) int {
		if a.k < b.k {
			return -1
		} else if a.k > b.k {
			return 1
		}
		return 0
	})
	for i := 0; i < n; i++ {
		keys[i] = pairs[i].k
		payloads[i] = pairs[i].p
	}
}

// SortParallel performs multi-threaded parallel sorting.
func SortParallel(data []uint32) {
	n := len(data)
	if n < 500000 {
		Sort(data)
		return
	}
	minV, maxV := data[0], data[0]
	for _, v := range data {
		if v < minV {
			minV = v
		}
		if v > maxV {
			maxV = v
		}
	}
	if minV == maxV {
		return
	}

	const K = 512
	rng := uint64(maxV) - uint64(minV)
	mult := ((uint64(K-1) << 32) + rng - 1) / rng

	numWorkers := 8
	chunk := (n + numWorkers - 1) / numWorkers
	threadCounts := make([][K]uint32, numWorkers)
	var wg sync.WaitGroup

	for t := 0; t < numWorkers; t++ {
		s := t * chunk
		e := s + chunk
		if e > n {
			e = n
		}
		if s >= n {
			break
		}
		wg.Add(1)
		go func(workerID, start, end int) {
			defer wg.Done()
			for i := start; i < end; i++ {
				b := (uint64(data[i]-minV) * mult) >> 32
				if b >= K {
					b = K - 1
				}
				threadCounts[workerID][b]++
			}
		}(t, s, e)
	}
	wg.Wait()

	totalCounts := make([]uint32, K)
	for b := 0; b < K; b++ {
		for t := 0; t < numWorkers; t++ {
			totalCounts[b] += threadCounts[t][b]
		}
	}

	starts := make([]uint32, K)
	threadOffsets := make([][K]uint32, numWorkers)
	var cur uint32
	for b := 0; b < K; b++ {
		starts[b] = cur
		for t := 0; t < numWorkers; t++ {
			threadOffsets[t][b] = cur
			cur += threadCounts[t][b]
		}
	}

	buf := make([]uint32, n)
	for t := 0; t < numWorkers; t++ {
		s := t * chunk
		e := s + chunk
		if e > n {
			e = n
		}
		if s >= n {
			break
		}
		wg.Add(1)
		go func(workerID, start, end int) {
			defer wg.Done()
			for i := start; i < end; i++ {
				v := data[i]
				b := (uint64(v-minV) * mult) >> 32
				if b >= K {
					b = K - 1
				}
				dest := threadOffsets[workerID][b]
				threadOffsets[workerID][b]++
				buf[dest] = v
			}
		}(t, s, e)
	}
	wg.Wait()

	for b := 0; b < K; b++ {
		cnt := totalCounts[b]
		if cnt == 0 {
			continue
		}
		st := starts[b]
		if cnt == 1 {
			data[st] = buf[st]
		} else {
			copy(data[st:st+cnt], buf[st:st+cnt])
			Sort(data[st : st+cnt])
		}
	}
}

// SortBy sorts a slice of custom structs using a key extraction function.
func SortBy[T any](data []T, keyFunc func(element *T) uint32) {
	n := len(data)
	if n <= 1 {
		return
	}
	type item struct {
		k uint32
		idx int
	}
	items := make([]item, n)
	for i := 0; i < n; i++ {
		items[i] = item{k: keyFunc(&data[i]), idx: i}
	}

	slices.SortFunc(items, func(a, b item) int {
		if a.k < b.k {
			return -1
		} else if a.k > b.k {
			return 1
		}
		return 0
	})

	temp := make([]T, n)
	copy(temp, data)
	for i := 0; i < n; i++ {
		data[i] = temp[items[i].idx]
	}
}
