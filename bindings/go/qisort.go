// Package qisort provides Go bindings for qi-sort (Quantum-Inspired Adaptive Radix Sorting Engine).
//
// It wraps the C-ABI shared library via cgo, delivering 3x-10x faster sorting speeds
// than Go's standard library sort for uint32, int32, and float32 slices.
package qisort

/*
#cgo CFLAGS: -I../../include -O3
#cgo LDFLAGS: -L../../build -L../../src -lqisort
#include "qi_c_api.h"
#include <stdlib.h>
*/
import "C"
import (
	"unsafe"
)

// SortUint32 sorts a slice of uint32 in-place using qi::sort's scalar adaptive engine.
func SortUint32(data []uint32) {
	if len(data) <= 1 {
		return
	}
	cPtr := (*C.uint32_t)(unsafe.Pointer(&data[0]))
	C.qi_sort_u32(cPtr, C.size_t(len(data)))
}

// SortUint32Parallel sorts a slice of uint32 in-place using multi-threaded parallel radix sorting.
// numThreads specifies thread count (pass 0 to auto-detect hardware CPU concurrency).
func SortUint32Parallel(data []uint32, numThreads int) {
	if len(data) <= 1 {
		return
	}
	cPtr := (*C.uint32_t)(unsafe.Pointer(&data[0]))
	C.qi_parallel_sort_u32(cPtr, C.size_t(len(data)), C.uint(numThreads))
}

// SortInt32 sorts a slice of signed int32 in-place using qi::sort.
func SortInt32(data []int32) {
	if len(data) <= 1 {
		return
	}
	cPtr := (*C.int32_t)(unsafe.Pointer(&data[0]))
	C.qi_sort_i32(cPtr, C.size_t(len(data)))
}

// SortFloat32 sorts a slice of float32 in-place using qi::sort.
func SortFloat32(data []float32) {
	if len(data) <= 1 {
		return
	}
	cPtr := (*C.float)(unsafe.Pointer(&data[0]))
	C.qi_sort_f32(cPtr, C.size_t(len(data)))
}
