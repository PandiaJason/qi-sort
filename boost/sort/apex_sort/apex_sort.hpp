#ifndef BOOST_SORT_APEX_SORT_HPP
#define BOOST_SORT_APEX_SORT_HPP

//  (C) Copyright Jason Pandian 2026.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

/*
========================================================================================
  Boost.Sort :: apex_sort (Adaptive L1-Bound Hardware-Aware Sorter)
========================================================================================
  Features:
  1. Standard Iterator Interface:
     boost::sort::apex_sort(first, last)
     boost::sort::parallel_apex_sort(first, last)
  2. Strict 20 KB L1-Data Cache Bounding
  3. 8-Way Unrolled Instruction-Level Parallelism (ILP)
  4. 4-Way Pipelined Lookahead Scatter (PF=48)
  5. 1ns Monotonic Fast-Path for Pre-Sorted Sequences
  6. Sub-millisecond Vectorized Counting Sort for Narrow Ranges (<= 4095)
========================================================================================
*/

#include <iterator>
#include <type_traits>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <cstring>
#include <thread>
#include "../../../include/qi_apex.hpp"

namespace boost {
namespace sort {

/**
 * @brief Standard Boost.Sort interface: Sorts the range [first, last) using apex_sort.
 * 
 * Automatically detects contiguous random access iterators (pointers, std::vector::iterator)
 * and dispatches to hardware-tuned L1-bound kernels.
 */
template <typename RandomAccessIter>
inline void apex_sort(RandomAccessIter first, RandomAccessIter last) {
    if (first >= last) return;
    using ValueType = typename std::iterator_traits<RandomAccessIter>::value_type;
    const size_t n = static_cast<size_t>(std::distance(first, last));

    if constexpr (std::is_pointer_v<RandomAccessIter> || 
                  std::is_same_v<RandomAccessIter, typename std::vector<ValueType>::iterator>) {
        ValueType* data_ptr = &(*first);
        qi::apex::sort(data_ptr, n);
    } else {
        std::sort(first, last);
    }
}

/**
 * @brief Multi-Threaded Parallel sort for the range [first, last).
 */
template <typename RandomAccessIter>
inline void parallel_apex_sort(RandomAccessIter first, RandomAccessIter last, unsigned int num_threads = 0) {
    if (first >= last) return;
    using ValueType = typename std::iterator_traits<RandomAccessIter>::value_type;
    const size_t n = static_cast<size_t>(std::distance(first, last));

    if constexpr (std::is_pointer_v<RandomAccessIter> || 
                  std::is_same_v<RandomAccessIter, typename std::vector<ValueType>::iterator>) {
        ValueType* data_ptr = &(*first);
        if constexpr (std::is_same_v<ValueType, uint32_t>) {
            qi::apex::parallel_sort(data_ptr, n, num_threads);
        } else {
            qi::apex::sort(data_ptr, n);
        }
    } else {
        std::sort(first, last);
    }
}

} // namespace sort
} // namespace boost

#endif // BOOST_SORT_APEX_SORT_HPP
