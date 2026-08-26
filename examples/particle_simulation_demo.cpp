/*
===============================================================================
COMPREHENSIVE RADIX & COMPARISON HEAD-TO-HEAD: 3D PARTICLE PHYSICS BENCHMARK
===============================================================================
Compares:
  1. std::sort (Struct)      — C++ Standard Library Introsort (Struct Array)
  2. pdqsort (Struct)        — Pattern-Defeating QuickSort (Rust std algorithm)
  3. ska_sort (Struct)       — Malte Skarupke's Official Radix Sorter (Struct Array)
  4. qi::sort_by (Struct)    — Quick Index Struct Sorting Engine (Full 28-Byte Structs)
  5. Plain Radix-8 (Keys)    — Fixed 4-Pass Radix Engine (32-bit Keys)
  6. Plain Radix-11 (Keys)   — Fixed 3-Pass Radix Engine (32-bit Keys)
  7. Plain Radix-16 (Keys)   — Fixed 2-Pass Radix Engine (32-bit Keys)
  8. qi::sort (Keys Only)    — Quick Index Adaptive Engine (32-bit Keys Only)

Evaluated on 250,000 active 3D Particles across 100 Physics Frames at 60 FPS Target.
===============================================================================
*/

#include "include/qi_radix.hpp"
#include "benchmarks/competitors/pdqsort.h"
#include "benchmarks/competitors/ska_sort.hpp"

#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <random>
#include <iomanip>
#include <algorithm>

struct Particle {
    float x, y, z;
    float vx, vy, vz;
    uint32_t spatial_key;
};

// Compute 32-bit Spatial Grid Hash (Morton Z-Order interleaving)
inline uint32_t compute_spatial_key(float x, float y, float z) {
    uint32_t ix = static_cast<uint32_t>(std::clamp(x * 1024.0f, 0.0f, 1023.0f));
    uint32_t iy = static_cast<uint32_t>(std::clamp(y * 1024.0f, 0.0f, 1023.0f));
    uint32_t iz = static_cast<uint32_t>(std::clamp(z * 1024.0f, 0.0f, 1023.0f));

    // Interleave bits (10 bits per dimension = 30 bits total)
    auto expand_bits = [](uint32_t v) {
        v = (v | (v << 16)) & 0x030000FF;
        v = (v | (v <<  8)) & 0x0300F00F;
        v = (v | (v <<  4)) & 0x030C30C3;
        v = (v | (v <<  2)) & 0x09249249;
        return v;
    };

    return expand_bits(ix) | (expand_bits(iy) << 1) | (expand_bits(iz) << 2);
}

int main() {
    const size_t NUM_PARTICLES = 250'000; // 250,000 Active 3D Particles
    const int NUM_FRAMES = 100;           // 100 Physics Frames

    std::cout << "=========================================================================\n";
    std::cout << "  3D PARTICLE PHYSICS BENCHMARK: STRUCT SORTING VS RADIX VARIANTS\n";
    std::cout << "  N = " << NUM_PARTICLES << " Particles | 100 Physics Frames | 60 FPS Target (16.6ms)\n";
    std::cout << "=========================================================================\n\n";

    std::cout << "Initializing 250,000 3D Particles in domain [0.0, 1.0]^3...\n";
    std::vector<Particle> particles(NUM_PARTICLES);
    std::vector<uint32_t> keys(NUM_PARTICLES);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> vel_dist(-0.01f, 0.01f);

    for (size_t i = 0; i < NUM_PARTICLES; ++i) {
        particles[i] = { dist(rng), dist(rng), dist(rng), vel_dist(rng), vel_dist(rng), vel_dist(rng), 0 };
    }

    double total_std_ms    = 0.0;
    double total_pdq_ms    = 0.0;
    double total_ska_ms    = 0.0;
    double total_qi_by_ms  = 0.0;
    double total_r8_key_ms = 0.0;
    double total_r11_key_ms= 0.0;
    double total_r16_key_ms= 0.0;
    double total_qi_key_ms = 0.0;

    std::cout << "Simulating 100 Physics Frames...\n\n";

    for (int frame = 0; frame < NUM_FRAMES; ++frame) {
        // 1. Physics Step: Integrate velocity & compute Spatial Keys
        for (size_t i = 0; i < NUM_PARTICLES; ++i) {
            particles[i].x += particles[i].vx;
            particles[i].y += particles[i].vy;
            particles[i].z += particles[i].vz;

            // Bounce boundary
            if (particles[i].x < 0.0f || particles[i].x > 1.0f) particles[i].vx *= -1.0f;
            if (particles[i].y < 0.0f || particles[i].y > 1.0f) particles[i].vy *= -1.0f;
            if (particles[i].z < 0.0f || particles[i].z > 1.0f) particles[i].vz *= -1.0f;

            uint32_t k = compute_spatial_key(particles[i].x, particles[i].y, particles[i].z);
            particles[i].spatial_key = k;
            keys[i] = k;
        }

        // A. std::sort (Particle Struct Array - 7 MB DRAM Move)
        auto p_std = particles;
        auto t0 = std::chrono::high_resolution_clock::now();
        std::sort(p_std.begin(), p_std.end(), [](const Particle& a, const Particle& b) {
            return a.spatial_key < b.spatial_key;
        });
        auto t1 = std::chrono::high_resolution_clock::now();
        total_std_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();

        // B. pdqsort (Particle Struct Array - 7 MB DRAM Move)
        auto p_pdq = particles;
        t0 = std::chrono::high_resolution_clock::now();
        pdqsort(p_pdq.begin(), p_pdq.end(), [](const Particle& a, const Particle& b) {
            return a.spatial_key < b.spatial_key;
        });
        t1 = std::chrono::high_resolution_clock::now();
        total_pdq_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();

        // C. ska_sort (Particle Struct Array - 7 MB DRAM Move)
        auto p_ska = particles;
        t0 = std::chrono::high_resolution_clock::now();
        ska_sort(p_ska.begin(), p_ska.end(), [](const Particle& p) {
            return p.spatial_key;
        });
        t1 = std::chrono::high_resolution_clock::now();
        total_ska_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();

        // D. qi::sort_by (Particle Struct Array - 7 MB DRAM Move)
        auto p_qi = particles;
        t0 = std::chrono::high_resolution_clock::now();
        qi::sort_by(p_qi, [](const Particle& p) { return p.spatial_key; });
        t1 = std::chrono::high_resolution_clock::now();
        total_qi_by_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();

        // E. Plain Radix-8 (Keys Only - 1 MB DRAM Move)
        auto k_r8 = keys;
        t0 = std::chrono::high_resolution_clock::now();
        qi::radix_8(k_r8.data(), k_r8.size());
        t1 = std::chrono::high_resolution_clock::now();
        total_r8_key_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();

        // F. Plain Radix-11 (Keys Only - 1 MB DRAM Move)
        auto k_r11 = keys;
        t0 = std::chrono::high_resolution_clock::now();
        qi::radix_11(k_r11.data(), k_r11.size());
        t1 = std::chrono::high_resolution_clock::now();
        total_r11_key_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();

        // G. Plain Radix-16 (Keys Only - 1 MB DRAM Move)
        auto k_r16 = keys;
        t0 = std::chrono::high_resolution_clock::now();
        qi::radix_16(k_r16.data(), k_r16.size());
        t1 = std::chrono::high_resolution_clock::now();
        total_r16_key_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();

        // H. qi::sort (Keys Only - 1 MB DRAM Move)
        auto k_qi = keys;
        t0 = std::chrono::high_resolution_clock::now();
        qi::sort(k_qi);
        t1 = std::chrono::high_resolution_clock::now();
        total_qi_key_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
    }

    double avg_std_ms     = total_std_ms     / NUM_FRAMES;
    double avg_pdq_ms     = total_pdq_ms     / NUM_FRAMES;
    double avg_ska_ms     = total_ska_ms     / NUM_FRAMES;
    double avg_qi_by_ms   = total_qi_by_ms   / NUM_FRAMES;
    double avg_r8_key_ms  = total_r8_key_ms  / NUM_FRAMES;
    double avg_r11_key_ms = total_r11_key_ms / NUM_FRAMES;
    double avg_r16_key_ms = total_r16_key_ms / NUM_FRAMES;
    double avg_qi_key_ms  = total_qi_key_ms  / NUM_FRAMES;

    std::cout << std::left << std::setw(34) << "Sorting Method"
              << std::setw(20) << "Data Payload Size"
              << std::setw(20) << "Avg Frame Time (ms)"
              << std::setw(15) << "vs std::sort" << "\n";
    std::cout << std::string(88, '-') << "\n";

    std::cout << std::left << std::setw(34) << "std::sort (Introsort)"
              << std::setw(20) << "28-Byte Structs (7MB)"
              << std::setw(20) << (std::to_string(avg_std_ms).substr(0, 5) + " ms")
              << "1.00x (Baseline)\n";

    std::cout << std::left << std::setw(34) << "pdqsort (Rust std algorithm)"
              << std::setw(20) << "28-Byte Structs (7MB)"
              << std::setw(20) << (std::to_string(avg_pdq_ms).substr(0, 5) + " ms")
              << (std::to_string(avg_std_ms / avg_pdq_ms).substr(0, 4) + "x\n");

    std::cout << std::left << std::setw(34) << "ska_sort (Skarupke Radix)"
              << std::setw(20) << "28-Byte Structs (7MB)"
              << std::setw(20) << (std::to_string(avg_ska_ms).substr(0, 5) + " ms")
              << (std::to_string(avg_std_ms / avg_ska_ms).substr(0, 4) + "x\n");

    std::cout << std::left << std::setw(34) << "qi::sort_by (Struct Sorter)"
              << std::setw(20) << "28-Byte Structs (7MB)"
              << std::setw(20) << (std::to_string(avg_qi_by_ms).substr(0, 5) + " ms")
              << (std::to_string(avg_std_ms / avg_qi_by_ms).substr(0, 4) + "x FASTER\n");

    std::cout << std::left << std::setw(34) << "Plain Radix-8 (Key Sorter)"
              << std::setw(20) << "4-Byte Keys (1MB)"
              << std::setw(20) << (std::to_string(avg_r8_key_ms).substr(0, 5) + " ms")
              << (std::to_string(avg_std_ms / avg_r8_key_ms).substr(0, 4) + "x FASTER\n");

    std::cout << std::left << std::setw(34) << "Plain Radix-11 (Key Sorter)"
              << std::setw(20) << "4-Byte Keys (1MB)"
              << std::setw(20) << (std::to_string(avg_r11_key_ms).substr(0, 5) + " ms")
              << (std::to_string(avg_std_ms / avg_r11_key_ms).substr(0, 4) + "x FASTER\n");

    std::cout << std::left << std::setw(34) << "Plain Radix-16 (Key Sorter)"
              << std::setw(20) << "4-Byte Keys (1MB)"
              << std::setw(20) << (std::to_string(avg_r16_key_ms).substr(0, 5) + " ms")
              << (std::to_string(avg_std_ms / avg_r16_key_ms).substr(0, 4) + "x FASTER\n");

    std::cout << std::left << std::setw(34) << "qi::sort (Key Sorter)"
              << std::setw(20) << "4-Byte Keys (1MB)"
              << std::setw(20) << (std::to_string(avg_qi_key_ms).substr(0, 5) + " ms")
              << (std::to_string(avg_std_ms / avg_qi_key_ms).substr(0, 4) + "x FASTER\n");

    std::cout << "\n=========================================================================\n";
    std::cout << "  SUCCESS: Struct vs Radix Variants Physics Benchmark Complete!\n";
    std::cout << "=========================================================================\n";

    return 0;
}
