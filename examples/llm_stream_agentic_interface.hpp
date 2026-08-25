#ifndef QI_LLM_STREAM_AGENTIC_INTERFACE_HPP
#define QI_LLM_STREAM_AGENTIC_INTERFACE_HPP

/**
 * qi-sort LLM, Agentic AI, and Stream Processing Interface
 * =========================================================
 * Production C++17 interfaces for:
 * 1. Stream Processing: Out-of-Order Watermark Window Sorting
 * 2. LLM Inference: Vocabulary Logit Top-K Token Sampling
 * 3. RAG / Vector DB: Cosine Similarity Score Top-K Reranking
 * 4. Agentic AI: Context Token Budget Memory Prioritization
 */

#include "include/qi_radix.hpp"
#include <vector>
#include <cstdint>
#include <string>
#include <algorithm>
#include <iostream>

namespace qi {
namespace ai {

// ============================================================================
// 1. LLM INFERENCE: VOCABULARY LOGIT TOP-K TOKEN SAMPLING
// ============================================================================

struct TokenLogit {
    uint32_t token_id;
    float logit_score;
};

// Sorts 32,000 to 128,000 vocabulary token logits for Top-K / Top-P sampling
inline void SampleTopKLogits(std::vector<TokenLogit>& logits) {
    // Quantize float logit score to uint32 for ultra-fast radix pass
    qi::sort_by(logits, [](const TokenLogit& t) {
        // Reverse order (highest logit score first)
        return ~key_traits::encode(t.logit_score);
    });
}

// ============================================================================
// 2. RAG & VECTOR DATABASE: SIMILARITY SCORE TOP-K RERANKING
// ============================================================================

struct VectorSearchResult {
    uint64_t doc_chunk_id;
    float similarity_score;
};

// Reranks vector search results by similarity score
inline void RerankVectorResults(std::vector<VectorSearchResult>& results) {
    qi::sort_by(results, [](const VectorSearchResult& r) {
        return ~key_traits::encode(r.similarity_score); // Highest similarity first
    });
}

// ============================================================================
// 3. AGENTIC AI: CONTEXT MEMORY TOKEN BUDGET PRIORITIZATION
// ============================================================================

struct AgentMemoryNode {
    uint64_t memory_id;
    float relevance_score;
    uint32_t token_cost;
    std::string content;
};

// Prioritizes agent memories by relevance score for prompt token budgeting
inline void PrioritizeAgentMemories(std::vector<AgentMemoryNode>& memories) {
    qi::sort_by(memories, [](const AgentMemoryNode& m) {
        return ~key_traits::encode(m.relevance_score); // Highest relevance first
    });
}

// ============================================================================
// 4. STREAM PROCESSING: OUT-OF-ORDER WATERMARK WINDOW SORTING
// ============================================================================

struct StreamEvent {
    uint64_t event_timestamp_ns;
    uint64_t sequence_id;
    uint32_t payload_hash;
};

// Sorts out-of-order streaming events by event timestamp before emitting watermark
inline void SortStreamWatermarkWindow(std::vector<StreamEvent>& events) {
    qi::sort_by(events, [](const StreamEvent& e) {
        return e.event_timestamp_ns;
    });
}

} // namespace ai
} // namespace qi

#endif // QI_LLM_STREAM_AGENTIC_INTERFACE_HPP
