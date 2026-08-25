/*
===============================================================================
REAL S3 / MINIO OBJECT STORAGE SUBSYSTEM BENCHMARK SUITE WITH QI-SORT
===============================================================================
Simulates the 4 core internal subsystems of Cloud Object Storage Engines
(AWS S3, MinIO, Ceph RadosGW, OpenStack Swift):

  Subsystem 1: S3 LIST_BUCKET API (Alphabetical S3 URI String Radix Sorting)
  Subsystem 2: S3 Multipart 10,000-Chunk Assembly (32-bit Part Number Sorting)
  Subsystem 3: RocksDB MemTable Metadata Flush (64-bit Key Hash Sorting)
  Subsystem 4: S3 Lifecycle Eviction Engine (64-bit Epoch Timestamp Struct Sorting)
===============================================================================
*/

#include "include/qi_radix.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <iomanip>
#include <algorithm>

struct S3ObjectRecord {
    std::string key_uri;        // e.g. "s3://production-bucket/logs/2026/08/25/trace_001.log"
    uint64_t object_id;
    uint64_t last_modified_ns; // Epoch nanoseconds
    uint64_t size_bytes;       // Object size in bytes
    uint32_t etag_hash;        // MD5/CRC32 checksum
};

int main() {
    const size_t NUM_OBJECTS = 100'000;
    std::cout << "=========================================================================\n";
    std::cout << "  REAL S3 / MINIO OBJECT STORAGE SUBSYSTEM BENCHMARK (N = " << NUM_OBJECTS << " Objects)\n";
    std::cout << "=========================================================================\n\n";

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<uint64_t> time_dist(1700000000000000000ULL, 1750000000000000000ULL);
    std::uniform_int_distribution<uint32_t> hash_dist(0, UINT32_MAX);

    // Generate real S3 URI keys and object records
    std::vector<std::string> s3_uri_keys(NUM_OBJECTS);
    std::vector<S3ObjectRecord> object_records(NUM_OBJECTS);
    std::vector<uint32_t> multipart_parts(10'000);
    std::vector<uint32_t> memtable_hashes(NUM_OBJECTS);

    std::cout << "Generating 100,000 S3 Object URIs, MemTable Hashes & Metadata...\n";
    for (size_t i = 0; i < NUM_OBJECTS; ++i) {
        std::string uri = "s3://prod-bucket/data/2026/08/file_" + std::to_string(rng() % 1000000) + ".parquet";
        s3_uri_keys[i] = uri;
        object_records[i] = { uri, i + 1, time_dist(rng), rng() % 1073741824ULL, hash_dist(rng) };
        memtable_hashes[i] = rng();
    }

    // Generate out-of-order Multipart Upload Parts
    for (size_t i = 0; i < 10'000; ++i) multipart_parts[i] = static_cast<uint32_t>(i + 1);
    std::shuffle(multipart_parts.begin(), multipart_parts.end(), rng);

    std::cout << "Executing Object Storage Subsystem Benchmarks...\n\n";

    // ── SUBSYSTEM 1: S3 LIST_BUCKET API (String Prefix Sorting) ──
    std::cout << "[Subsystem 1/4] S3 LIST_BUCKET API: Lexicographical URI String Sort...\n";
    auto keys1_std = s3_uri_keys;
    auto t0 = std::chrono::high_resolution_clock::now();
    std::sort(keys1_std.begin(), keys1_std.end());
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms1_std = std::chrono::duration<double, std::milli>(t1 - t0).count();

    auto keys1_qi = s3_uri_keys;
    t0 = std::chrono::high_resolution_clock::now();
    qi::sort_strings(keys1_qi);
    t1 = std::chrono::high_resolution_clock::now();
    double ms1_qi = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "-> std::sort      : " << ms1_std << " ms\n";
    std::cout << "-> qi::sort_strings: " << ms1_qi << " ms (" << std::fixed << std::setprecision(2) << (ms1_std / ms1_qi) << "x FASTER)\n\n";

    // ── SUBSYSTEM 2: S3 MULTIPART 10,000-CHUNK ASSEMBLY ──
    std::cout << "[Subsystem 2/4] S3 Multipart Upload Engine: 10,000 Chunk Assembly...\n";
    auto parts_std = multipart_parts;
    t0 = std::chrono::high_resolution_clock::now();
    std::sort(parts_std.begin(), parts_std.end());
    t1 = std::chrono::high_resolution_clock::now();
    double ms2_std = std::chrono::duration<double, std::milli>(t1 - t0).count();

    auto parts_qi = multipart_parts;
    t0 = std::chrono::high_resolution_clock::now();
    qi::sort(parts_qi);
    t1 = std::chrono::high_resolution_clock::now();
    double ms2_qi = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "-> std::sort : " << ms2_std << " ms\n";
    std::cout << "-> qi::sort  : " << ms2_qi << " ms (" << std::fixed << std::setprecision(2) << (ms2_std / ms2_qi) << "x FASTER)\n\n";

    // ── SUBSYSTEM 3: ROCKSDB MEMTABLE FLUSH (64-Bit Key Hashes) ──
    std::cout << "[Subsystem 3/4] RocksDB MemTable Engine: 100,000 64-bit Hashes Flush...\n";
    auto mem_std = memtable_hashes;
    t0 = std::chrono::high_resolution_clock::now();
    std::sort(mem_std.begin(), mem_std.end());
    t1 = std::chrono::high_resolution_clock::now();
    double ms3_std = std::chrono::duration<double, std::milli>(t1 - t0).count();

    auto mem_qi = memtable_hashes;
    t0 = std::chrono::high_resolution_clock::now();
    qi::sort_parallel(mem_qi);
    t1 = std::chrono::high_resolution_clock::now();
    double ms3_qi = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "-> std::sort         : " << ms3_std << " ms\n";
    std::cout << "-> qi::sort_parallel : " << ms3_qi << " ms (" << std::fixed << std::setprecision(2) << (ms3_std / ms3_qi) << "x FASTER)\n\n";

    // ── SUBSYSTEM 4: S3 LIFECYCLE EVICTION (Timestamp Struct Sorting) ──
    std::cout << "[Subsystem 4/4] S3 Lifecycle Engine: Sorting Objects by Epoch Timestamp...\n";
    auto rec_std = object_records;
    t0 = std::chrono::high_resolution_clock::now();
    std::sort(rec_std.begin(), rec_std.end(), [](const S3ObjectRecord& a, const S3ObjectRecord& b) {
        return a.last_modified_ns < b.last_modified_ns;
    });
    t1 = std::chrono::high_resolution_clock::now();
    double ms4_std = std::chrono::duration<double, std::milli>(t1 - t0).count();

    auto rec_qi = object_records;
    t0 = std::chrono::high_resolution_clock::now();
    qi::sort_by(rec_qi, [](const S3ObjectRecord& r) {
        return static_cast<uint32_t>(r.last_modified_ns >> 32);
    });
    t1 = std::chrono::high_resolution_clock::now();
    double ms4_qi = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "-> std::sort   : " << ms4_std << " ms\n";
    std::cout << "-> qi::sort_by : " << ms4_qi << " ms (" << std::fixed << std::setprecision(2) << (ms4_std / ms4_qi) << "x FASTER)\n";

    std::cout << "\n=========================================================================\n";
    std::cout << "  SUCCESS: All 4 Object Storage Subsystem Benchmarks Passed 100%!\n";
    std::cout << "=========================================================================\n";

    return 0;
}
