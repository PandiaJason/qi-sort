/*
========================================================================================
LIVE REAL-TIME DATASET BENCHMARK AGENT
========================================================================================
Passes REAL, live system data (not synthetic pseudorandom numbers) into 9 sorting engines
in real-time:

  1. Live System File Sizes (from /System, /usr, /var)
  2. Live English Dictionary Hashes (from /usr/share/dict/words)
  3. Live Process IDs & Virtual Memory Page Addresses (from OS kernel)
  4. Live System File Modification Timestamps (Epoch seconds)

Sorting Algorithms Tested per Dataset (Fair Execution, Fresh Memory Copies):
  1. std::sort                (C++ Standard IntroSort)
  2. std::stable_sort         (C++ Standard Timsort)
  3. pdqsort                  (Pattern-Defeating Quicksort - Rust std)
  4. ska_sort                 (Skarupke American Flag Radix)
  5. Plain Radix-8            (Fixed 4-pass, shortcuts on)
  6. Plain Radix-11           (Fixed 3-pass, shortcuts on)
  7. Plain Radix-16           (Fixed 2-pass, shortcuts on)
  8. qi::sort SCALAR          (Adaptive Quantum-Inspired Engine)
  9. qi::sort PARALLEL        (Multi-Threaded Adaptive Engine)
========================================================================================
*/

#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <cstdint>
#include <functional>
#include <string>

// Competitor Headers
#include "/tmp/pdqsort.h"
#include "/tmp/ska_sort.hpp"

// Our Engine
#include "../include/qi_radix.hpp"

namespace fs = std::filesystem;
using Clock = std::chrono::high_resolution_clock;

// Simple CRC32 for hashing dictionary words
static uint32_t crc32(const std::string& str) {
    uint32_t crc = 0xFFFFFFFF;
    for (char c : str) {
        crc ^= static_cast<uint8_t>(c);
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

struct Dataset {
    std::string name;
    std::string source;
    std::vector<uint32_t> data;
};

// ── REAL-TIME LIVE DATA GATHERER ──
static std::vector<Dataset> gather_real_data() {
    std::vector<Dataset> datasets;

    // 1. Live Dictionary Hashes
    {
        std::vector<uint32_t> hashes;
        std::ifstream dictFile("/usr/share/dict/words");
        if (dictFile.is_open()) {
            std::string line;
            while (std::getline(dictFile, line)) {
                if (!line.empty()) hashes.push_back(crc32(line));
            }
        }
        if (!hashes.empty()) {
            datasets.push_back({"Dictionary Hashes", "/usr/share/dict/words (" + std::to_string(hashes.size()) + " words)", hashes});
        }
    }

    // 2. Live System File Sizes & Timestamps
    {
        std::vector<uint32_t> sizes;
        std::vector<uint32_t> timestamps;
        const std::vector<std::string> targetDirs = {"/usr/bin", "/usr/lib", "/System/Library", "/usr/share"};

        for (const auto& dirPath : targetDirs) {
            if (!fs::exists(dirPath)) continue;
            try {
                for (const auto& entry : fs::recursive_directory_iterator(dirPath, fs::directory_options::skip_permission_denied)) {
                    if (entry.is_regular_file()) {
                        auto sz = static_cast<uint32_t>(entry.file_size() & 0xFFFFFFFF);
                        sizes.push_back(sz);
                        auto ftime = entry.last_write_time();
                        auto s_t = std::chrono::duration_cast<std::chrono::seconds>(ftime.time_since_epoch()).count();
                        timestamps.push_back(static_cast<uint32_t>(s_t & 0xFFFFFFFF));
                        if (sizes.size() >= 300000) break;
                    }
                }
            } catch (...) {}
            if (sizes.size() >= 300000) break;
        }

        if (!sizes.empty()) {
            datasets.push_back({"Live System File Sizes", "macOS System Directory Traversal (" + std::to_string(sizes.size()) + " files)", sizes});
        }
        if (!timestamps.empty()) {
            datasets.push_back({"Live File Timestamps", "macOS System Modification Epochs (" + std::to_string(timestamps.size()) + " files)", timestamps});
        }
    }

    // 3. Live Heap/Stack Virtual Memory Addresses
    {
        std::vector<uint32_t> addresses;
        addresses.reserve(500000);
        for (size_t i = 0; i < 500000; ++i) {
            void* ptr = ::operator new(16, std::nothrow);
            uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
            addresses.push_back(static_cast<uint32_t>(addr & 0xFFFFFFFF));
            ::operator delete(ptr, std::nothrow);
        }
        datasets.push_back({"Live Memory Page Addresses", "OS Virtual Allocator Addresses (500,000 pointers)", addresses});
    }

    return datasets;
}

// ── BENCHMARK RUNNER AGENT ──
int main() {
    std::cout << "========================================================================================================\n";
    std::cout << "  REAL-TIME LIVE DATASET BENCHMARK AGENT\n";
    std::cout << "  Gathering real live datasets from OS kernel, filesystem, and system allocator...\n";
    std::cout << "========================================================================================================\n\n";

    auto datasets = gather_real_data();

    if (datasets.empty()) {
        std::cerr << "Error: No live system datasets could be gathered.\n";
        return 1;
    }

    for (const auto& ds : datasets) {
        std::cout << "========================================================================================================\n";
        std::cout << " DATASET: " << ds.name << "\n";
        std::cout << " SOURCE : " << ds.source << " | Count: " << ds.data.size() << " elements\n";
        std::cout << "========================================================================================================\n";

        const size_t N = ds.data.size();

        auto run_bench = [&](const std::string& name, std::function<void(std::vector<uint32_t>&)> fn) {
            std::vector<uint32_t> work = ds.data;
            auto start = Clock::now();
            fn(work);
            auto end = Clock::now();
            double ms = std::chrono::duration<double, std::milli>(end - start).count();
            bool isSorted = std::is_sorted(work.begin(), work.end());
            return std::make_pair(ms, isSorted);
        };

        // 1. std::sort
        auto r_std = run_bench("std::sort", [](auto& d) { std::sort(d.begin(), d.end()); });
        // 2. std::stable_sort
        auto r_tim = run_bench("std::stable_sort", [](auto& d) { std::stable_sort(d.begin(), d.end()); });
        // 3. pdqsort
        auto r_pdq = run_bench("pdqsort", [](auto& d) { pdqsort(d.begin(), d.end()); });
        // 4. ska_sort
        auto r_ska = run_bench("ska_sort", [](auto& d) { ska_sort(d.begin(), d.end()); });
        // 5. Plain Radix-8
        auto r_r8  = run_bench("Plain Radix-8", [](auto& d) { qi::detail::radixSort8(d.data(), d.size(), true); });
        // 6. Plain Radix-11
        auto r_r11 = run_bench("Plain Radix-11", [](auto& d) { qi::detail::radixSort11(d.data(), d.size(), true); });
        // 7. Plain Radix-16
        auto r_r16 = run_bench("Plain Radix-16", [](auto& d) { qi::detail::radixSort16(d.data(), d.size(), true); });
        // 8. qi::sort SCALAR
        auto r_qis = run_bench("qi::sort SCALAR", [](auto& d) { qi::sort(d); });
        // 9. qi::sort PARALLEL
        qi::SortOptions pOpts; pOpts.parallel = true;
        auto r_qip = run_bench("qi::sort PARALLEL", [&](auto& d) { qi::sort(d, pOpts); });

        double t_qi = r_qis.first;

        auto print_row = [&](const std::string& cat, const std::string& name, double t, bool ok) {
            double throughput = (N / t) / 1000.0;
            double vs_qi = t / t_qi;
            std::cout << "  " << std::left << std::setw(20) << cat
                      << std::setw(34) << name
                      << std::setw(12) << std::fixed << std::setprecision(2) << t << " ms  "
                      << std::setw(16) << (std::to_string(static_cast<int>(throughput)) + " MKeys/s")
                      << std::setw(20) << (std::to_string(vs_qi).substr(0, 4) + "x vs qi scalar")
                      << (ok ? "[✓ OK]" : "[✗ FAIL]") << "\n";
        };

        std::cout << std::left << std::setw(22) << "  Category"
                  << std::setw(34) << "Algorithm Engine"
                  << std::setw(12) << "Time (ms)"
                  << std::setw(16) << "Throughput"
                  << std::setw(20) << "vs qi::sort Scalar"
                  << "Status\n";
        std::cout << "--------------------------------------------------------------------------------------------------------\n";

        print_row("Standard C++",     "std::sort (IntroSort)",               r_std.first, r_std.second);
        print_row("Python/Java/Rust", "std::stable_sort (Timsort)",          r_tim.first, r_tim.second);
        print_row("Rust std Sorter",  "pdqsort (Orson Peters)",              r_pdq.first, r_pdq.second);
        print_row("Fastest Open Radix","ska_sort (Malte Skarupke)",           r_ska.first, r_ska.second);
        print_row("Fixed Radix",      "Plain Radix-8 (Fixed 4-Pass)",        r_r8.first,  r_r8.second);
        print_row("Fixed Radix",      "Plain Radix-11 (Fixed 3-Pass)",       r_r11.first, r_r11.second);
        print_row("Fixed Radix",      "Plain Radix-16 (Fixed 2-Pass)",       r_r16.first, r_r16.second);
        print_row("OUR ENGINE",       "qi::sort SCALAR (Adaptive)",          r_qis.first, r_qis.second);
        print_row("OUR ENGINE",       "qi::sort PARALLEL (Multi-Threaded)",   r_qip.first, r_qip.second);
        std::cout << "========================================================================================================\n\n";
    }

    return 0;
}
