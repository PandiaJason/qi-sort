package qisort

import (
	"fmt"
	"sync"
	"time"
)

// LiveStreamProcessor handles real-time live IoT telemetry streams with sliding windows.
type LiveStreamProcessor struct {
	windowSize int
	mutex      sync.Mutex
	incoming   []SensorReading
	stopChan   chan struct{}
}

// NewLiveStreamProcessor creates a live stream ingestion processor for IoT sensors.
func NewLiveStreamProcessor(windowSize int) *LiveStreamProcessor {
	return &LiveStreamProcessor{
		windowSize: windowSize,
		incoming:   make([]SensorReading, 0, windowSize*2),
		stopChan:   make(chan struct{}),
	}
}

// IngestLiveReading ingests a live real-time IoT packet from MQTT/Modbus/UDP.
func (p *LiveStreamProcessor) IngestLiveReading(timestampNS uint64, sensorID uint32, value float32) {
	p.mutex.Lock()
	defer p.mutex.Unlock()
	p.incoming = append(p.incoming, SensorReading{
		TimestampNS: timestampNS,
		SensorID:    sensorID,
		Value:       value,
	})
}

// StartProcessingLoop starts the background window processing loop.
func (p *LiveStreamProcessor) StartProcessingLoop(onSortedWindow func(batch []SensorReading, elapsedMS float64)) {
	go func() {
		ticker := time.NewTicker(200 * time.Millisecond)
		defer ticker.Stop()

		for {
			select {
			case <-p.stopChan:
				return
			case <-ticker.C:
				var batch []SensorReading
				p.mutex.Lock()
				if len(p.incoming) >= p.windowSize {
					batch = p.incoming
					p.incoming = make([]SensorReading, 0, p.windowSize*2)
				}
				p.mutex.Unlock()

				if len(batch) > 0 {
					t0 := time.Now()
					SortTelemetryByTimestamp(batch)
					elapsedMS := time.Since(t0).Seconds() * 1000.0
					if onSortedWindow != nil {
						onSortedWindow(batch, elapsedMS)
					} else {
						fmt.Printf("[LIVE GO IIOT ENGINE] Processed & Sorted Window Batch of %d readings in %.4f ms\n", len(batch), elapsedMS)
					}
				}
			}
		}
	}()
}

// Stop stops the live processor loop.
func (p *LiveStreamProcessor) Stop() {
	close(p.stopChan)
}
