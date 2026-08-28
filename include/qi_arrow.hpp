#ifndef QI_ARROW_HPP
#define QI_ARROW_HPP

/*
========================================================================================
  qi::arrow: Zero-Copy Apache Arrow Columnar Acceleration Engine (C++17)
========================================================================================
  Provides zero-dependency in-place acceleration for Apache Arrow C Data Interface.
  Compatible with PyArrow, Polars, DuckDB, Pandas 2.0, and Apache DataFusion.
========================================================================================
*/

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include "qi_apex.hpp"

// ════════════════════════════════════════════════════════════════════════════
// 1. Official Apache Arrow C Data Interface ABI (Zero-Dependency Definition)
//    Specification: https://arrow.apache.org/docs/format/CDataInterface.html
// ════════════════════════════════════════════════════════════════════════════

#ifndef ARROW_C_DATA_INTERFACE
#define ARROW_C_DATA_INTERFACE

#define ARROW_FLAG_DICTIONARY_ORDERED 1
#define ARROW_FLAG_NULLABLE 2
#define ARROW_FLAG_MAP_KEYS_SORTED 4

struct ArrowSchema {
    // Array type description
    const char* format;
    const char* name;
    const char* metadata;
    int64_t flags;
    int64_t n_children;
    struct ArrowSchema** children;
    struct ArrowSchema* dictionary;

    // Release callback
    void (*release)(struct ArrowSchema*);
    // Opaque producer-specific data
    void* private_data;
};

struct ArrowArray {
    // Array data description
    int64_t length;
    int64_t null_count;
    int64_t offset;
    int64_t n_buffers;
    int64_t n_children;
    const void** buffers;
    struct ArrowArray** children;
    struct ArrowArray* dictionary;

    // Release callback
    void (*release)(struct ArrowArray*);
    // Opaque producer-specific data
    void* private_data;
};

#endif // ARROW_C_DATA_INTERFACE

namespace qi {
namespace arrow {

// ════════════════════════════════════════════════════════════════════════════
// 2. Arrow Array Zero-Copy Sorter
// ════════════════════════════════════════════════════════════════════════════

/**
 * @brief Sorts an Apache Arrow Array in-place zero-copy using qi::apex.
 * 
 * Supports:
 * - "I": uint32
 * - "i": int32
 * - "f": float32 (IEEE 754)
 * - "L": uint64
 * - "l": int64 (and Timestamp/Duration)
 * - "g": float64 / double
 * - "tdD": Date32 (Days since UNIX epoch)
 * - "ttm": Time32 (Milliseconds)
 */
inline void sort_array(ArrowArray* array, const ArrowSchema* schema) {
    if (!array || !schema || array->length <= 1) return;
    if (array->offset != 0) {
        throw std::runtime_error("qi::arrow: non-zero offset arrays must be sliced first");
    }
    if (array->n_buffers < 2 || !array->buffers[1]) {
        throw std::runtime_error("qi::arrow: array has no data buffer");
    }

    const char* fmt = schema->format;
    if (!fmt) throw std::runtime_error("qi::arrow: schema format is null");

    size_t n = static_cast<size_t>(array->length);
    void* data_ptr = const_cast<void*>(array->buffers[1]);

    // ── 32-bit Unsigned Integer ("I") ──
    if (std::strcmp(fmt, "I") == 0) {
        qi::apex::sort(reinterpret_cast<uint32_t*>(data_ptr), n);
    }
    // ── 32-bit Signed Integer ("i"), Date32 ("tdD"), Time32 ("ttm") ──
    else if (std::strcmp(fmt, "i") == 0 || std::strcmp(fmt, "tdD") == 0 || std::strcmp(fmt, "ttm") == 0) {
        qi::apex::sort(reinterpret_cast<int32_t*>(data_ptr), n);
    }
    // ── 32-bit Float ("f") ──
    else if (std::strcmp(fmt, "f") == 0) {
        qi::apex::sort(reinterpret_cast<float*>(data_ptr), n);
    }
    // ── 64-bit Unsigned Integer ("L") ──
    else if (std::strcmp(fmt, "L") == 0) {
        qi::apex::sort(reinterpret_cast<uint64_t*>(data_ptr), n);
    }
    // ── 64-bit Signed Integer ("l"), Timestamp ("ts...") ──
    else if (std::strcmp(fmt, "l") == 0 || std::strncmp(fmt, "ts", 2) == 0) {
        qi::apex::sort(reinterpret_cast<int64_t*>(data_ptr), n);
    }
    // ── 64-bit Float / Double ("g") ──
    else if (std::strcmp(fmt, "g") == 0) {
        qi::apex::sort(reinterpret_cast<double*>(data_ptr), n);
    }
    else {
        throw std::runtime_error(std::string("qi::arrow: unsupported type format: ") + fmt);
    }
}

/**
 * @brief Multi-Threaded Parallel sort for an Apache Arrow Array.
 */
inline void parallel_sort_array(ArrowArray* array, const ArrowSchema* schema, unsigned int num_threads = 0) {
    if (!array || !schema || array->length <= 1) return;
    const char* fmt = schema->format;
    if (!fmt) return;

    size_t n = static_cast<size_t>(array->length);
    void* data_ptr = const_cast<void*>(array->buffers[1]);

    if (std::strcmp(fmt, "I") == 0) {
        qi::apex::parallel_sort(reinterpret_cast<uint32_t*>(data_ptr), n, num_threads);
    } else {
        sort_array(array, schema);
    }
}

/**
 * @brief Sorts a Key Column and a 64-bit Payload Column (e.g. Row IDs) in-place together.
 *        Accelerates Database ORDER BY queries.
 */
inline void sort_table_by_u32(ArrowArray* key_array, ArrowArray* payload_array) {
    if (!key_array || !payload_array || key_array->length <= 1) return;
    if (key_array->length != payload_array->length) {
        throw std::runtime_error("qi::arrow: key and payload column length mismatch");
    }

    size_t n = static_cast<size_t>(key_array->length);
    uint32_t* keys = const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(key_array->buffers[1]));
    uint64_t* payloads = const_cast<uint64_t*>(reinterpret_cast<const uint64_t*>(payload_array->buffers[1]));

    qi::apex::sort_pairs(keys, payloads, n);
}

} // namespace arrow
} // namespace qi

#endif // QI_ARROW_HPP
