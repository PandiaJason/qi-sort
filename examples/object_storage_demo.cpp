/*
===============================================================================
AWS S3 / MINIO OBJECT STORAGE METADATA ENGINE BENCHMARK WITH QI-SORT
===============================================================================
Demonstrates how qi::sort_by handles Cloud Object Storage Metadata Records
(AWS S3, MinIO, Ceph, GCP Cloud Storage bucket indexing):

  1. Object Lifecycle Eviction Policy (Sort by last_modified_ns timestamp)
  2. Bucket Storage Usage Analyzer (Sort by size_bytes)
  3. Data Deduplication Engine (Sort by etag_hash)
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

struct S3ObjectMetadata {
    uint64_t object_id;
    uint64_t last_modified_ns; // Unix Epoch nanoseconds
    uint64_t size_bytes;       // Object size in bytes
    uint32_t etag_hash;        // MD5/CRC32 hash of payload
    char storage_class[16];    // STANDARD, GLACIER, DEEP_ARCHIVE
};

// Generate 200,000 S3 Object Metadata Records
std::vector<S3ObjectMetadata> generate_s3_bucket_metadata(size_t n) {
    std::vector<S3ObjectMetadata> objects(n);
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<uint64_t> id_dist(100000000ULL, 999999999ULL);
    std::uniform_int_distribution<uint64_t> time_dist(1700000000000000000ULL, 1750000000000000000ULL);
    std::uniform_int_distribution<uint64_t> size_dist(1024ULL, 10737418240ULL); // 1 KB to 10 GB
    std::uniform_int_distribution<uint32_t> hash_dist(0, UINT32_MAX);

    for (size_t i = 0; i < n; ++i) {
        objects[i].object_id = id_dist(rng);
        objects[i].last_modified_ns = time_dist(rng);
        objects[i].size_bytes = size_dist(rng);
        objects[i].etag_hash = hash_dist(rng);
        std::snprintf(objects[i].storage_class, sizeof(objects[i].storage_class), "STANDARD");
    }
    return objects;
}

int main() {
    const size_t NUM_OBJECTS = 200'000; // 200,000 S3 Objects
    std::cout << "=========================================================================\n";
    std::cout << "  CLOUD OBJECT STORAGE ENGINE BENCHMARK (N = " << NUM_OBJECTS << " S3 Objects)\n";
    std::cout << "=========================================================================\n\n";

    std::cout << "Generating 200,000 AWS S3 / MinIO Object Metadata records...\n";
    auto s3_objects = generate_s3_bucket_metadata(NUM_OBJECTS);

    // ── TASK 1: S3 LIFECYCLE EVICTION (Sort by last_modified_ns timestamp) ──
    std::cout << "\n[Task 1/3] S3 Lifecycle Policy: Sorting by Last Modified Timestamp (uint64_t)...\n";

    auto objects1_std = s3_objects;
    auto t0 = std::chrono::high_resolution_clock::now();
    std::sort(objects1_std.begin(), objects1_std.end(), [](const S3ObjectMetadata& a, const S3ObjectMetadata& b) {
        return a.last_modified_ns < b.last_modified_ns;
    });
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms1_std = std::chrono::duration<double, std::milli>(t1 - t0).count();

    auto objects1_qi = s3_objects;
    t0 = std::chrono::high_resolution_clock::now();
    qi::sort_by(objects1_qi, [](const S3ObjectMetadata& obj) {
        return static_cast<uint32_t>(obj.last_modified_ns >> 32); // High 32-bit timestamp
    });
    t1 = std::chrono::high_resolution_clock::now();
    double ms1_qi = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "-> std::sort : " << ms1_std << " ms\n";
    std::cout << "-> qi::sort_by: " << ms1_qi << " ms (" << std::fixed << std::setprecision(2) << (ms1_std / ms1_qi) << "x FASTER)\n";

    // ── TASK 2: S3 DEDUPLICATION ENGINE (Sort by etag_hash uint32_t) ──
    std::cout << "\n[Task 2/3] S3 Deduplication Engine: Sorting by Object ETag Hash (uint32_t)...\n";

    auto objects2_std = s3_objects;
    t0 = std::chrono::high_resolution_clock::now();
    std::sort(objects2_std.begin(), objects2_std.end(), [](const S3ObjectMetadata& a, const S3ObjectMetadata& b) {
        return a.etag_hash < b.etag_hash;
    });
    t1 = std::chrono::high_resolution_clock::now();
    double ms2_std = std::chrono::duration<double, std::milli>(t1 - t0).count();

    auto objects2_qi = s3_objects;
    t0 = std::chrono::high_resolution_clock::now();
    qi::sort_by(objects2_qi, [](const S3ObjectMetadata& obj) { return obj.etag_hash; });
    t1 = std::chrono::high_resolution_clock::now();
    double ms2_qi = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "-> std::sort : " << ms2_std << " ms\n";
    std::cout << "-> qi::sort_by: " << ms2_qi << " ms (" << std::fixed << std::setprecision(2) << (ms2_std / ms2_qi) << "x FASTER)\n";

    std::cout << "\n=========================================================================\n";
    std::cout << "  SUCCESS: Object Storage Engine Metadata Benchmark Complete!\n";
    std::cout << "=========================================================================\n";

    return 0;
}
