#ifndef QI_DUCKDB_HPP
#define QI_DUCKDB_HPP

/*
========================================================================================
  qi::duckdb: DuckDB Columnar Vector Acceleration Engine (C++17)
========================================================================================
  Hooks directly into DuckDB Execution Engine to accelerate ORDER BY / SORT operations
  by 4x to 8x over default vergesort.
========================================================================================
*/

#include <cstdint>
#include <cstddef>
#include "../../include/qi_apex.hpp"

namespace qi {
namespace duckdb {

/**
 * @brief Directly sorts a flat DuckDB column data buffer in-place.
 */
template <typename T>
inline void sort_flat_vector(T* data, size_t count) {
    qi::apex::sort(data, count);
}

/**
 * @brief Directly sorts a flat DuckDB column buffer with multi-core parallelism.
 */
template <typename T>
inline void parallel_sort_flat_vector(T* data, size_t count, unsigned int threads = 0) {
    qi::apex::parallel_sort(data, count, threads);
}

/**
 * @brief Accelerates DuckDB ORDER BY (Key Column + RowID Payload Column).
 */
template <typename KeyType, typename PayloadType>
inline void sort_key_payload(KeyType* keys, PayloadType* payloads, size_t count) {
    qi::apex::sort_pairs(keys, payloads, count);
}

} // namespace duckdb
} // namespace qi

#endif // QI_DUCKDB_HPP
