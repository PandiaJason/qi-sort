package qisort_test

import (
	"math/rand"
	"slices"
	"testing"

	"github.com/PandiaJason/qi-sort/bindings/go"
)

func TestSort(t *testing.T) {
	const N = 50000
	rng := rand.New(rand.NewSource(42))
	data := make([]uint32, N)
	for i := 0; i < N; i++ {
		data[i] = rng.Uint32()
	}

	qisort.Sort(data)
	if !slices.IsSorted(data) {
		t.Fatalf("qisort.Sort failed to sort uint32 slice correctly")
	}
}

func BenchmarkQISort_2M(b *testing.B) {
	const N = 2_000_000
	rng := rand.New(rand.NewSource(42))
	orig := make([]uint32, N)
	for i := 0; i < N; i++ {
		orig[i] = rng.Uint32()
	}

	data := make([]uint32, N)
	b.ResetTimer()

	for i := 0; i < b.N; i++ {
		b.StopTimer()
		copy(data, orig)
		b.StartTimer()
		qisort.Sort(data)
	}
}

func BenchmarkSlicesSort_2M(b *testing.B) {
	const N = 2_000_000
	rng := rand.New(rand.NewSource(42))
	orig := make([]uint32, N)
	for i := 0; i < N; i++ {
		orig[i] = rng.Uint32()
	}

	data := make([]uint32, N)
	b.ResetTimer()

	for i := 0; i < b.N; i++ {
		b.StopTimer()
		copy(data, orig)
		b.StartTimer()
		slices.Sort(data)
	}
}
