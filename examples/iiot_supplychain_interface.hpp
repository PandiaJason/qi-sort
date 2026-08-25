#ifndef QI_IIOT_SUPPLYCHAIN_INTERFACE_HPP
#define QI_IIOT_SUPPLYCHAIN_INTERFACE_HPP

/**
 * qi-sort Industrial IoT & Supply Chain Interface
 * ===============================================
 * Production-ready C++17 interfaces for:
 * 1. High-frequency IoT Sensor Telemetry Time-Series Sorting
 * 2. Supply Chain Geospatial Package Delivery Route Clustering
 */

#include "include/qi_radix.hpp"
#include <vector>
#include <cstdint>
#include <string>
#include <cmath>
#include <iostream>

namespace qi {
namespace iiot {

// ============================================================================
// 1. INDUSTRIAL IOT TIME-SERIES SENSOR TELEMETRY INTERFACE
// ============================================================================

struct SensorReading {
    uint64_t timestamp_ns; // Nanosecond timestamp
    uint32_t sensor_id;    // Industrial sensor ID
    float value;           // Telemetry value (temperature, vibration, etc.)
};

class TelemetryIngestBuffer {
public:
    void AddReading(uint64_t timestamp_ns, uint32_t sensor_id, float value) {
        readings_.push_back({timestamp_ns, sensor_id, value});
    }

    // Ultra-fast time-series sorting by nanosecond timestamp using qi::sort_by
    void SortByTimestamp() {
        qi::sort_by(readings_, [](const SensorReading& r) {
            return r.timestamp_ns;
        });
    }

    const std::vector<SensorReading>& GetReadings() const { return readings_; }
    size_t Size() const { return readings_.size(); }
    void Clear() { readings_.clear(); }

private:
    std::vector<SensorReading> readings_;
};

// ============================================================================
// 2. SUPPLY CHAIN GEOSPATIAL PACKAGE ROUTE CLUSTERING INTERFACE
// ============================================================================

struct PackageDelivery {
    uint64_t package_id;
    float latitude;
    float longitude;
    uint32_t morton_key; // 32-bit Morton Spatial Key (Z-Order Curve)
};

// Converts 2D Latitude/Longitude GPS coordinates into a 32-bit Morton Key
inline uint32_t EncodeMorton2D(float lat, float lon) {
    // Normalize lat [-90, 90] and lon [-180, 180] to [0, 65535]
    uint32_t x = static_cast<uint32_t>(((lat + 90.0f) / 180.0f) * 65535.0f);
    uint32_t y = static_cast<uint32_t>(((lon + 180.0f) / 360.0f) * 65535.0f);

    auto dilate = [](uint32_t v) -> uint32_t {
        v = (v | (v << 8)) & 0x00FF00FFu;
        v = (v | (v << 4)) & 0x0F0F0F0Fu;
        v = (v | (v << 2)) & 0x33333333u;
        v = (v | (v << 1)) & 0x55555555u;
        return v;
    };

    return (dilate(x) << 1) | dilate(y);
}

class LogisticsRouteClusterEngine {
public:
    void AddPackage(uint64_t package_id, float latitude, float longitude) {
        uint32_t morton = EncodeMorton2D(latitude, longitude);
        packages_.push_back({package_id, latitude, longitude, morton});
    }

    // Clusters packages spatially along Morton Z-order curve using qi::sort_by
    void ClusterDeliveryRoutes() {
        qi::sort_by(packages_, [](const PackageDelivery& p) {
            return p.morton_key;
        });
    }

    const std::vector<PackageDelivery>& GetClusteredPackages() const { return packages_; }
    size_t Size() const { return packages_.size(); }

private:
    std::vector<PackageDelivery> packages_;
};

} // namespace iiot
} // namespace qi

#endif // QI_IIOT_SUPPLYCHAIN_INTERFACE_HPP
