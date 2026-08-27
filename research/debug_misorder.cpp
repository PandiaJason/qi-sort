#include <iostream>
#include <vector>
#include <random>
#include "qi_hybrid_engine.hpp"

int main() {
    size_t N = 10000000;
    std::mt19937_64 rng(1337);
    std::vector<uint32_t> original(N);
    for (size_t i = 0; i < N; ++i) original[i] = rng();

    auto data = original;
    qi_hybrid::parallel_sort(data.data(), N);

    for (size_t i = 1; i < N; ++i) {
        if (data[i] < data[i - 1]) {
            std::cout << "MISORDER at index " << i << ": data[" << i - 1 << "] = " 
                      << data[i - 1] << " (hex: 0x" << std::hex << data[i - 1] << ") > data[" 
                      << std::dec << i << "] = " << data[i] << " (hex: 0x" << std::hex << data[i] << ")\n";
            return 1;
        }
    }
    std::cout << "PERFECTLY SORTED!\n";
    return 0;
}
