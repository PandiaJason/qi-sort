package qisort_test

import (
	"math/rand"
	"slices"
	"testing"

	"github.com/PandiaJason/qi-sort/bindings/go"
)

type Employee struct {
	ID   uint32
	Name string
}

func TestSortBy(t *testing.T) {
	employees := []Employee{
		{ID: 105, Name: "Charlie"},
		{ID: 100, Name: "Bob"},
		{ID: 102, Name: "Alice"},
	}

	qisort.SortBy(employees, func(e *Employee) uint32 {
		return e.ID
	})

	if employees[0].ID != 100 || employees[1].ID != 102 || employees[2].ID != 105 {
		t.Fatalf("qisort.SortBy failed to sort Employee slice by ID")
	}
}

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

func TestSortCPP(t *testing.T) {
	const N = 50000
	rng := rand.New(rand.NewSource(42))
	data := make([]uint32, N)
	for i := 0; i < N; i++ {
		data[i] = rng.Uint32()
	}

	qisort.SortCPP(data)
	if !slices.IsSorted(data) {
		t.Fatalf("qisort.SortCPP failed to sort uint32 slice correctly")
	}
}

func BenchmarkQISortGo_2M(b *testing.B) {
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

func BenchmarkQISortCPP_2M(b *testing.B) {
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
		qisort.SortCPP(data)
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
