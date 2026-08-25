#include "examples/iiot_supplychain_interface.hpp"
#include <iostream>

int main() {
    std::cout << "=== Industrial IoT & Supply Chain qi-sort Interface Demo ===\n\n";

    // 1. Industrial IoT Time-Series Telemetry Example
    qi::iiot::TelemetryIngestBuffer telemetry;
    telemetry.AddReading(1700000005000000ULL, 101, 74.5f);
    telemetry.AddReading(1700000001000000ULL, 102, 72.1f);
    telemetry.AddReading(1700000003000000ULL, 101, 75.0f);
    telemetry.AddReading(1700000002000000ULL, 103, 80.2f);

    std::cout << "Sorting 4 Industrial IoT Telemetry Readings by Timestamp:\n";
    telemetry.SortByTimestamp();

    for (const auto& r : telemetry.GetReadings()) {
        std::cout << "  Timestamp (ns): " << r.timestamp_ns << " | Sensor ID: " << r.sensor_id << " | Val: " << r.value << "\n";
    }

    // 2. Supply Chain Geospatial Package Delivery Route Clustering Example
    qi::iiot::LogisticsRouteClusterEngine logistics;
    logistics.AddPackage(9001, 37.7749f, -122.4194f); // San Francisco
    logistics.AddPackage(9002, 34.0522f, -118.2437f); // Los Angeles
    logistics.AddPackage(9003, 37.7833f, -122.4167f); // SF Nearby
    logistics.AddPackage(9004, 34.0500f, -118.2500f); // LA Nearby

    std::cout << "\nClustering 4 Supply Chain Packages Spatially (Morton Z-Order):\n";
    logistics.ClusterDeliveryRoutes();

    for (const auto& p : logistics.GetClusteredPackages()) {
        std::cout << "  Package ID: " << p.package_id << " | Lat: " << p.latitude << ", Lon: " << p.longitude << " | Morton Key: " << p.morton_key << "\n";
    }

    return 0;
}
