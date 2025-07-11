#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace fast_pdf_parser {

struct DoclingMeta {
    std::string schema_name = "docling_core.transforms.chunker.DocMeta";
    std::string version = "1.0.0";
    
    struct Origin {
        std::string mimetype = "application/pdf";
        int64_t binary_hash;
        std::string filename;
        std::string uri;
    };
    
    Origin origin;
    std::vector<nlohmann::json> doc_items;
    std::vector<std::string> headings;
    nlohmann::json captions = nullptr;
};

class JsonSerializer {
public:
    // Convert raw extraction output to Docling-compatible format
    static nlohmann::json to_docling_format(const nlohmann::json& raw_output,
                                           const std::string& filename,
                                           int64_t file_hash);
    
    // Chunk text according to Docling's HybridChunker behavior
    static std::vector<nlohmann::json> chunk_document(const nlohmann::json& document,
                                                     size_t max_tokens = 512,
                                                     bool merge_peers = true);
    
    // Serialize chunks to JSON array format
    static std::string serialize_chunks(const std::vector<nlohmann::json>& chunks);

private:
    static size_t estimate_tokens(const std::string& text);
    static std::vector<std::string> extract_headings(const nlohmann::json& doc_items);
};

} // namespace fast_pdf_parser