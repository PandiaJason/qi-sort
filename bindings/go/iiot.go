package qisort

// SensorReading represents an Industrial IoT time-series telemetry packet.
type SensorReading struct {
	TimestampNS uint64  // Nanosecond timestamp
	SensorID    uint32  // Sensor ID
	Value       float32 // Sensor reading value
}

// PackageDelivery represents a supply chain delivery location.
type PackageDelivery struct {
	PackageID uint64
	Latitude  float32
	Longitude float32
	MortonKey uint32
}

// SortTelemetryByTimestamp sorts sensor telemetry readings by timestamp.
func SortTelemetryByTimestamp(readings []SensorReading) {
	SortBy(readings, func(r *SensorReading) uint32 {
		return uint32(r.TimestampNS & 0xFFFFFFFF)
	})
}

// EncodeMorton2D converts latitude and longitude into a 32-bit Morton Spatial Key.
func EncodeMorton2D(lat, lon float32) uint32 {
	x := uint32(((lat + 90.0) / 180.0) * 65535.0)
	y := uint32(((lon + 180.0) / 360.0) * 65535.0)

	dilate := func(v uint32) uint32 {
		v = (v | (v << 8)) & 0x00FF00FF
		v = (v | (v << 4)) & 0x0F0F0F0F
		v = (v | (v << 2)) & 0x33333333
		v = (v | (v << 1)) & 0x55555555
		return v
	}

	return (dilate(x) << 1) | dilate(y)
}

// ClusterPackagesSpatially clusters packages along a 2D Morton Z-Order curve.
func ClusterPackagesSpatially(packages []PackageDelivery) {
	for i := range packages {
		packages[i].MortonKey = EncodeMorton2D(packages[i].Latitude, packages[i].Longitude)
	}
	SortBy(packages, func(p *PackageDelivery) uint32 {
		return p.MortonKey
	})
}
