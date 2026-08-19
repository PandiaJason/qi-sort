/*
===============================================================================
QI-RADIX EXAMPLE 2: DISTRIBUTION ANALYSIS (ANALYTICAL INSPECTION)
===============================================================================
Demonstrates using qi::analyze() to inspect distribution features (entropy,
IPR, effective states, duplicate ratio, bit masks) without modifying data.
===============================================================================
*/

#include <iostream>
#include <vector>
#include <random>
#include <iomanip>
#include "../include/qi_radix.hpp"

int main() {
    std::cout << "=== QI-Radix Analytical Inspection Example ===\n\n";

    // Generate a sample dataset (100,000 low-range 16-bit integers)
    size_t N = 100000;
    std::vector<uint32_t> dataset(N);
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, 65535);

    for (auto& x : dataset) x = dist(rng);

    // Perform non-destructive analysis
    qi::State state = qi::analyze(dataset);

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Dataset Size           : " << N << " elements\n";
    std::cout << "Sampled Elements       : " << state.sampleSize << "\n";
    std::cout << "Analysis Time          : " << state.analysisTimeMs << " ms\n";
    std::cout << "--------------------------------------------------------\n";
    std::cout << "Average Shannon Entropy: " << state.averageEntropy << " (0.0=constant, 1.0=random)\n";
    std::cout << "Amplitude Conc. (IPR)  : " << state.amplitudeConcentration << "\n";
    std::cout << "Effective States N_eff : " << state.effectiveStates << " buckets/byte\n";
    std::cout << "Duplicate Ratio        : " << state.duplicateRatio * 100.0 << "%\n";
    std::cout << "Pre-sorted Orderedness : " << state.orderedness * 100.0 << "%\n";
    std::cout << "Recommended Radix      : " 
              << (state.recommendedRadix == qi::Radix::R16 ? "RADIX-16" : 
                 (state.recommendedRadix == qi::Radix::R11 ? "RADIX-11" : "RADIX-8")) << "\n\n";

    std::cout << "Byte-level breakdown:\n";
    for (int i = 0; i < 4; ++i) {
        std::cout << " Byte " << i 
                  << " | Entropy: " << state.bytes[i].entropy
                  << " | IPR: " << state.bytes[i].amplitudeConcentration
                  << " | Occupied Buckets: " << state.bytes[i].occupied << "/256\n";
    }

    return 0;
}
