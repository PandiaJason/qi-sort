package qisort

import "math"

// TokenLogit represents an LLM vocabulary token with its unnormalized logit score.
type TokenLogit struct {
	TokenID    uint32
	LogitScore float32
}

// VectorSearchResult represents a RAG vector database document chunk similarity score.
type VectorSearchResult struct {
	DocChunkID      uint64
	SimilarityScore float32
}

// AgentMemoryNode represents an Agentic AI episodic memory item with relevance score.
type AgentMemoryNode struct {
	MemoryID       uint64
	RelevanceScore float32
	TokenCost      uint32
	Content        string
}

// StreamEvent represents an out-of-order event stream packet.
type StreamEvent struct {
	EventTimestampNS uint64
	SequenceID       uint64
	PayloadHash      uint32
}

// SampleTopKLogits sorts LLM vocabulary token logits in descending probability order.
func SampleTopKLogits(logits []TokenLogit) {
	SortBy(logits, func(t *TokenLogit) uint32 {
		// Encode float logit so higher logits come first (bit inverted)
		bits := math.Float32bits(t.LogitScore)
		if bits&0x80000000 != 0 {
			bits = ^bits
		} else {
			bits ^= 0x80000000
		}
		return ^bits
	})
}

// RerankVectorResults reranks RAG vector search results by descending similarity score.
func RerankVectorResults(results []VectorSearchResult) {
	SortBy(results, func(r *VectorSearchResult) uint32 {
		bits := math.Float32bits(r.SimilarityScore)
		if bits&0x80000000 != 0 {
			bits = ^bits
		} else {
			bits ^= 0x80000000
		}
		return ^bits
	})
}

// PrioritizeAgentMemories prioritizes Agentic AI context memories by relevance score.
func PrioritizeAgentMemories(memories []AgentMemoryNode) {
	SortBy(memories, func(m *AgentMemoryNode) uint32 {
		bits := math.Float32bits(m.RelevanceScore)
		if bits&0x80000000 != 0 {
			bits = ^bits
		} else {
			bits ^= 0x80000000
		}
		return ^bits
	})
}

// SortStreamWatermarkWindow sorts out-of-order streaming events by event timestamp.
func SortStreamWatermarkWindow(events []StreamEvent) {
	SortBy(events, func(e *StreamEvent) uint32 {
		return uint32(e.EventTimestampNS & 0xFFFFFFFF)
	})
}
