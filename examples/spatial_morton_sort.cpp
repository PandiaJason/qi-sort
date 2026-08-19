/*
===============================================================================
QI-RADIX EXAMPLE 4: 3D GRAPHICS & RAY-TRACING (MORTON CODE SORTING)
===============================================================================
Demonstrates sorting 32-bit Morton Z-order spatial curve codes for Bounding
Volume Hierarchy (BVH) construction in game engines and ray tracing.
===============================================================================
*/

#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include "../include/qi_radix.hpp"

// Helper function to interleave 10-bit integer coordinates into 30-bit Morton code
static uint32_t encodeMorton3D(uint32_t x, uint32_t y, uint32_t z) {
    auto expandBits = [](uint32_t v) {
        v = (v | (v << 16)) & 0x030000FF;
        v = (v | (v <<  8)) & 0x0300F00F;
        v = (v | (v <<  4)) & 0x030C30C3;
        v = (v | (v <<  2)) & 0x09249249;
        return v;
    };
    return (expandBits(x) << 2) | (expandBits(y) << 1) | expandBits(z);
}

int main() {
    std::cout << "=== QI-Radix Spatial Morton Code BVH Sort Example ===\n\n";

    size_t particleCount = 1000000;
    std::vector<uint32_t> mortonCodes(particleCount);

    std::mt19937 rng(999);
    std::uniform_int_distribution<uint32_t> coordDist(0, 1023); // 10-bit grid coordinates

    for (size_t i = 0; i < particleCount; ++i) {
        uint32_t px = coordDist(rng);
        uint32_t py = coordDist(rng);
        uint32_t pz = coordDist(rng);
        mortonCodes[i] = encodeMorton3D(px, py, pz);
    }

    std::cout << "Generated " << particleCount << " 3D spatial Morton keys.\n";
    std::cout << "Sorting Morton keys for BVH tree generation...\n";

    auto start = std::chrono::high_resolution_clock::now();

    // Sort 1,000,000 spatial Morton codes adaptively
    qi::sort(mortonCodes);

    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "BVH Morton key sorting completed in " << ms << " ms!\n";
    std::cout << "Morton key spatial hierarchy ready for GPU ray-tracing acceleration.\n";

    return 0;
}
