#include "examples/llm_stream_agentic_interface.hpp"
#include <iostream>

int main() {
    std::cout << "=== LLM Inference, RAG, Agentic AI & Stream Processing Demo ===\n\n";

    // 1. LLM Vocabulary Logit Top-K Token Sampling Demo
    std::vector<qi::ai::TokenLogit> logits = {
        {1052, 2.14f}, {304, 8.91f}, {991, 0.42f}, {812, 12.35f}, {4401, 5.70f}
    };
    qi::ai::SampleTopKLogits(logits);

    std::cout << "1. LLM Top-K Token Logits (Highest Probability First):\n";
    for (size_t i = 0; i < logits.size(); ++i) {
        std::cout << "   Rank " << i+1 << ": Token ID = " << logits[i].token_id << " | Logit Score = " << logits[i].logit_score << "\n";
    }

    // 2. RAG & Vector Database Cosine Similarity Reranking Demo
    std::vector<qi::ai::VectorSearchResult> rag_docs = {
        {1001, 0.72f}, {1002, 0.95f}, {1003, 0.61f}, {1004, 0.88f}
    };
    qi::ai::RerankVectorResults(rag_docs);

    std::cout << "\n2. RAG Vector Search Reranking (Highest Similarity First):\n";
    for (const auto& doc : rag_docs) {
        std::cout << "   Doc Chunk ID = " << doc.doc_chunk_id << " | Similarity = " << doc.similarity_score << "\n";
    }

    // 3. Agentic AI Context Token Budget Memory Prioritization Demo
    std::vector<qi::ai::AgentMemoryNode> memories = {
        {1, 0.65f, 150, "User prefers Python code examples"},
        {2, 0.98f, 200, "Goal: Optimize sorting engine for Linux"},
        {3, 0.82f, 120, "Workspace: qi-sort project directory"}
    };
    qi::ai::PrioritizeAgentMemories(memories);

    std::cout << "\n3. Agentic AI Memory Prioritization (Prompt Token Budgeting):\n";
    for (const auto& mem : memories) {
        std::cout << "   [Score: " << mem.relevance_score << "] " << mem.content << " (Tokens: " << mem.token_cost << ")\n";
    }

    // 4. Stream Processing Watermark Event Window Sorting Demo
    std::vector<qi::ai::StreamEvent> stream_events = {
        {1700000003000000ULL, 103, 0xA1},
        {1700000001000000ULL, 101, 0xB2},
        {1700000002000000ULL, 102, 0xC3}
    };
    qi::ai::SortStreamWatermarkWindow(stream_events);

    std::cout << "\n4. Stream Processing Watermark Event Window (Sorted by Timestamp):\n";
    for (const auto& event : stream_events) {
        std::cout << "   Timestamp: " << event.event_timestamp_ns << " ns | Seq ID: " << event.sequence_id << "\n";
    }

    return 0;
}
