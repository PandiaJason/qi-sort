package qisort_test

import (
	"math/rand"
	"slices"
	"testing"
	"time"

	"qisort"
)

func TestSortUint32(t *testing.T) {
	data := []uint32{10543, 42, 999999, 12, 0, 8881}
	qisort.SortUint32(data)

	if !slices.IsSorted(data) {
		t.Fatalf("Uint32 slice is not sorted: %v", data)
	}
}

func TestSortInt32(t *testing.T) {
	data := []int32{-500, 42, -10, 1000, 0}
	qisort.SortInt32(data)

	if !slices.IsSorted(data) {
		t.Fatalf("Int32 slice is not sorted: %v", data)
	}
}

func TestSortFloat32(t *testing.T) {
	data := []float32{-3.14, 100.5, 0.0, 2.71, -100.0}
	qisort.SortFloat32(data)

	if !slices.IsSorted(data) {
		t.Fatalf("Float32 slice is not sorted: %v", data)
	}
}

func TestSortUint32Parallel(t *testing.T) {
	rng := rand.New(rand.NewSource(42))
	n := 500000
	data := make([]uint32, n)
	for i := 0; i < n; i++ {
		data[i] = rng.Uint32()
	}

	qisort.SortUint32Parallel(data, 0)

	if !slices.IsSorted(data) {
		t.Fatalf("Parallel uint32 slice is not sorted")
	}
}

func BenchmarkGoStandardSort(b *testing.B) {
	rng := rand.New(rand.NewSource(time.Now().UnixNano()))
	n := 1000000
	raw := make([]uint32, n)
	for i := 0; i < n; i++ {
		raw[i] = rng.Uint32()
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		data := make([]uint32, n)
		copy(data, raw)
		slices.Sort(data)
	}
}

func BenchmarkQiSortScalar(b *testing.B) {
	rng := rand.New(rand.NewSource(time.Now().UnixNano()))
	n := 1000000
	raw := make([]uint32, n)
	for i := 0; i < n; i++ {
		raw[i] = rng.Uint32()
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		data := make([]uint32, n)
		copy(data, raw)
		qisort.SortUint32(data)
	}
}

func BenchmarkQiSortParallel(b *testing.B) {
	rng := rand.New(rand.NewSource(time.Now().UnixNano()))
	n := 1000000
	raw := make([]uint32, n)
	for i := 0; i < n; i++ {
		raw[i] = rng.Uint32()
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		data := make([]uint32, n)
		copy(data, raw)
		qisort.SortUint32Parallel(data, 0)
	}
}
