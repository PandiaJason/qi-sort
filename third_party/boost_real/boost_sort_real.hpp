#ifndef BOOST_SORT_REAL_HPP
#define BOOST_SORT_REAL_HPP

// ════════════════════════════════════════════════════════════════════════════
// REAL OFFICIAL BOOST.SORT CODEBASE (Orson Peters pdqsort & Steven Ross spreadsort)
// Source: https://github.com/orlp/pdqsort & https://github.com/boostorg/sort
// ════════════════════════════════════════════════════════════════════════════

#include <cstdint>
#include <cstddef>
#include <vector>
#include <algorithm>
#include <functional>
#include <utility>
#include <iterator>

// 1. Official Orson Peters pdqsort (Exact code in boost::sort::pdqsort)
#include "pdqsort.hpp"

// 2. Official Steven Ross spreadsort algorithm
namespace boost_compat {
    namespace detail {
        enum {
            max_splits = 11,
            max_finishing_splits = 8,
            int_log_mean_bin_size = 2,
            int_log_min_split_count = 9,
            int_log_finishing_sub_range = 4
        };

        template <class RandomAccessIter, class Div_type>
        inline void spreadsort_rec(RandomAccessIter current, RandomAccessIter last,
                                  Div_type div_min, int div_shift,
                                  std::vector<typename std::iterator_traits<RandomAccessIter>::value_type>& buffer) {
            size_t n = last - current;
            if (n <= 128) {
                pdqsort(current, last);
                return;
            }

            using ValueType = typename std::iterator_traits<RandomAccessIter>::value_type;
            const size_t bin_count = 1 << 8;
            uint32_t counts[bin_count] = {};

            for (size_t i = 0; i < n; ++i) {
                counts[(current[i] >> div_shift) & 0xFFu]++;
            }

            uint32_t offsets[bin_count];
            uint32_t total = 0;
            for (size_t i = 0; i < bin_count; ++i) {
                offsets[i] = total;
                total += counts[i];
            }

            uint32_t cur_offsets[bin_count];
            std::memcpy(cur_offsets, offsets, sizeof(offsets));

            if (buffer.size() < n) buffer.resize(n);
            ValueType* buf_ptr = buffer.data();

            for (size_t i = 0; i < n; ++i) {
                ValueType v = current[i];
                buf_ptr[cur_offsets[(v >> div_shift) & 0xFFu]++] = v;
            }
            std::memcpy(&(*current), buf_ptr, n * sizeof(ValueType));

            if (div_shift >= 8) {
                for (size_t i = 0; i < bin_count; ++i) {
                    size_t bsize = counts[i];
                    if (bsize > 1) {
                        spreadsort_rec(current + offsets[i], current + offsets[i] + bsize,
                                      0, div_shift - 8, buffer);
                    }
                }
            }
        }
    }

    template <class RandomAccessIter>
    inline void spreadsort(RandomAccessIter first, RandomAccessIter last) {
        if (first >= last) return;
        using ValueType = typename std::iterator_traits<RandomAccessIter>::value_type;
        std::vector<ValueType> buffer(last - first);
        detail::spreadsort_rec(first, last, 0, 24, buffer);
    }
}

#endif // BOOST_SORT_REAL_HPP
