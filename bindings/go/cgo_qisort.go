package qisort

/*
#cgo CFLAGS: -I../../include
#cgo LDFLAGS: -L${SRCDIR} -lqisort -lstdc++
#include "qi_c_api.h"
*/
import "C"
import "unsafe"

// SortCPP sorts a uint32 slice using the native C++ engine via CGO.
func SortCPP(data []uint32) {
	if len(data) <= 1 {
		return
	}
	ptr := (*C.uint32_t)(unsafe.Pointer(&data[0]))
	n := C.size_t(len(data))
	C.qi_sort_u32(ptr, n)
}
