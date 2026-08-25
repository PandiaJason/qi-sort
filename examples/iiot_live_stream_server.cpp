#include "examples/iiot_supplychain_interface.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <random>

namespace qi {
namespace iiot {

// ============================================================================
// REAL-TIME LIVE STREAM SLIDING-WINDOW IIOT ENGINE
// ============================================================================

class LiveStreamProcessor {
public:
    LiveStreamProcessor(size_t window_size_samples)
        : window_size_(window_size_samples), running_(false) {}

    ~LiveStreamProcessor() { Stop(); }

    // Start live streaming ingestion thread
    void Start() {
        running_ = true;
        processing_thread_ = std::thread(&LiveStreamProcessor::ProcessLoop, this);
    }

    // Stop live streaming ingestion thread
    void Stop() {
        if (running_) {
            running_ = false;
            if (processing_thread_.joinable()) {
                processing_thread_.join();
            }
        }
    }

    // Ingest live telemetry frame from MQTT / Modbus / UDP socket
    void IngestReading(uint64_t timestamp_ns, uint32_t sensor_id, float value) {
        std::lock_guard<std::mutex> lock(mutex_);
        incoming_buffer_.push_back({timestamp_ns, sensor_id, value});
    }

private:
    void ProcessLoop() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 500ms window tick

            std::vector<SensorReading> window_batch;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (incoming_buffer_.size() >= window_size_) {
                    window_batch.swap(incoming_buffer_);
                }
            }

            if (!window_batch.empty()) {
                auto t0 = std::chrono::high_resolution_clock::now();

                // Sort real-time live window batch using qi::sort_by
                qi::sort_by(window_batch, [](const SensorReading& r) {
                    return r.timestamp_ns;
                });

                auto t1 = std::chrono::high_resolution_clock::now();
                double sort_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

                std::cout << "[LIVE IIOT ENGINE] Processed & Sorted Window Batch of "
                          << window_batch.size() << " readings in " << sort_ms << " ms | "
                          << "First timestamp: " << window_batch.front().timestamp_ns << " ns | "
                          << "Last timestamp: " << window_batch.back().timestamp_ns << " ns\n";
            }
        }
    }

    size_t window_size_;
    std::atomic<bool> running_;
    std::mutex mutex_;
    std::vector<SensorReading> incoming_buffer_;
    std::thread processing_thread_;
};

} // namespace iiot
} // namespace qi

int main() {
    std::cout << "=== Real-Time Live Streaming Industrial IoT Data Processor ===\n\n";

    qi::iiot::LiveStreamProcessor live_processor(100); // 100-sample window
    live_processor.Start();

    // Simulate live incoming MQTT / SCADA telemetry stream from 4 factory machines
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> sensor_dist(100, 104);
    std::uniform_real_distribution<float> temp_dist(65.0f, 95.0f);

    uint64_t base_timestamp = 1700000000000000ULL;

    std::cout << "Simulating live incoming stream of 300 telemetry readings...\n\n";
    for (int i = 0; i < 300; ++i) {
        // Out-of-order live timestamps (network jitter simulation)
        uint64_t jitter = rng() % 5000;
        uint64_t ts = base_timestamp + (i * 1000) + jitter;
        uint32_t sensor = sensor_dist(rng);
        float temp = temp_dist(rng);

        live_processor.IngestReading(ts, sensor, temp);
        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Live stream interval
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    live_processor.Stop();

    std::cout << "\n[LIVE IIOT ENGINE] Live stream processing completed successfully.\n";
    return 0;
}
