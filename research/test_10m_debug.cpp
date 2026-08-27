#include <iostream>
#include <vector>
#include <random>
#include "qi_hybrid_engine.hpp"

int main() {
    size_t N = 10000000;
    std::vector<uint32_t> data(N);
    std::mt19937_64 rng(42);
    for (size_t i = 0; i < N; ++i) data[i] = rng();

    std::cout << "Sorting 10M elements with parallel_sort...\n";
    qi_hybrid::parallel_sort(data.data(), N);

    bool sorted = true;
    for (size_t i = 1; i < N; ++i) {
        if (data[i] < data[i - 1]) {
            std::cout << "FAIL at index " << i << ": data[" << i - 1 << "] = "
                      << data[i - 1] << " > data[" << i << "] = " << data[i] << "\n";
            sorted = false;
            break;
        }
    }
    if (sorted) {
        std::cout << "SUCCESS: 10M elements perfectly sorted!\n";
    }
    return 0;
}
