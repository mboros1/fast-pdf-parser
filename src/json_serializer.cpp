#include "fast_pdf_parser/json_serializer.h"
#include <algorithm>
#include <sstream>
#include <regex>

namespace fast_pdf_parser {

nlohmann::json JsonSerializer::to_docling_format(const nlohmann::json& raw_output,
                                                const std::string& filename,
                                                int64_t file_hash) {
    nlohmann::json docling_doc;
    docling_doc["doc_items"] = nlohmann::json::array();
    
    // Process each page
    for (const auto& page : raw_output["pages"]) {
        if (page.contains("error")) {
            continue; // Skip error pages
        }
        
        // Process each block in the page
        for (const auto& block : page["blocks"]) {
            nlohmann::json doc_item;
            doc_item["type"] = "text_block";
            doc_item["page_number"] = page["page_number"];
            
            std::string block_text;
            std::vector<nlohmann::json> char_positions;
            
            // Process each line in the block
            for (const auto& line : block["lines"]) {
                if (!block_text.empty()) {
                    block_text += "\n";
                }
                block_text += line["text"];
                
                // Collect character positions if available
                if (line.contains("chars")) {
                    for (const auto& ch : line["chars"]) {
                        nlohmann::json char_info;
                        char_info["char"] = ch["char"];
                        
                        if (ch.contains("bbox")) {
                            char_info["bbox"] = ch["bbox"];
                        }
                        if (ch.contains("font")) {
                            char_info["font"] = ch["font"];
                        }
                        if (ch.contains("size")) {
                            char_info["size"] = ch["size"];
                        }
                        
                        char_positions.push_back(char_info);
                    }
                }
            }
            
            doc_item["text"] = block_text;
            
            if (!char_positions.empty()) {
                doc_item["char_positions"] = char_positions;
            }
            
            if (block.contains("bbox")) {
                doc_item["bbox"] = block["bbox"];
            }
            
            docling_doc["doc_items"].push_back(doc_item);
        }
    }
    
    // Create metadata
    DoclingMeta meta;
    meta.origin.filename = filename;
    meta.origin.binary_hash = file_hash;
    meta.doc_items = docling_doc["doc_items"];
    meta.headings = extract_headings(docling_doc["doc_items"]);
    
    // Build final document structure
    nlohmann::json result;
    result["content"] = docling_doc;
    result["meta"] = {
        {"schema_name", meta.schema_name},
        {"version", meta.version},
        {"origin", {
            {"mimetype", meta.origin.mimetype},
            {"binary_hash", meta.origin.binary_hash},
            {"filename", meta.origin.filename},
            {"uri", nullptr}
        }},
        {"doc_items", meta.doc_items},
        {"headings", meta.headings},
        {"captions", meta.captions}
    };
    
    return result;
}

std::vector<nlohmann::json> JsonSerializer::chunk_document(const nlohmann::json& document,
                                                          size_t max_tokens,
                                                          bool merge_peers) {
    std::vector<nlohmann::json> chunks;
    
    if (!document.contains("content") || !document["content"].contains("doc_items")) {
        return chunks;
    }
    
    const auto& doc_items = document["content"]["doc_items"];
    nlohmann::json current_chunk;
    current_chunk["text"] = "";
    current_chunk["meta"] = document["meta"];
    current_chunk["meta"]["doc_items"] = nlohmann::json::array();
    
    size_t current_tokens = 0;
    
    for (const auto& item : doc_items) {
        std::string item_text = item["text"];
        size_t item_tokens = estimate_tokens(item_text);
        
        // If adding this item would exceed max_tokens, start a new chunk
        if (current_tokens > 0 && current_tokens + item_tokens > max_tokens) {
            chunks.push_back(current_chunk);
            
            // Start new chunk
            current_chunk["text"] = "";
            current_chunk["meta"]["doc_items"] = nlohmann::json::array();
            current_tokens = 0;
        }
        
        // Add item to current chunk
        if (!current_chunk["text"].get<std::string>().empty()) {
            current_chunk["text"] = current_chunk["text"].get<std::string>() + "\n\n";
        }
        current_chunk["text"] = current_chunk["text"].get<std::string>() + item_text;
        current_chunk["meta"]["doc_items"].push_back(item);
        current_tokens += item_tokens;
    }
    
    // Add final chunk if not empty
    if (current_tokens > 0) {
        chunks.push_back(current_chunk);
    }
    
    // Update headings for each chunk based on its content
    for (auto& chunk : chunks) {
        chunk["meta"]["headings"] = extract_headings(chunk["meta"]["doc_items"]);
    }
    
    return chunks;
}

std::string JsonSerializer::serialize_chunks(const std::vector<nlohmann::json>& chunks) {
    nlohmann::json output = nlohmann::json::array();
    
    for (const auto& chunk : chunks) {
        output.push_back(chunk);
    }
    
    return output.dump(2);
}

size_t JsonSerializer::estimate_tokens(const std::string& text) {
    // Simple token estimation: ~4 characters per token
    // This matches typical tokenizer behavior for English text
    return (text.length() + 3) / 4;
}

std::vector<std::string> JsonSerializer::extract_headings(const nlohmann::json& doc_items) {
    std::vector<std::string> headings;
    
    for (const auto& item : doc_items) {
        std::string text = item["text"];
        
        // Simple heuristic: lines that are short and don't end with punctuation
        // could be headings. In production, use font size/style from char_positions
        std::istringstream stream(text);
        std::string line;
        
        while (std::getline(stream, line)) {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            
            // Check if likely heading
            if (!line.empty() && line.length() < 100) {
                char last = line.back();
                if (last != '.' && last != ',' && last != ';' && last != ':') {
                    // Additional check: if font info available, use it
                    if (item.contains("char_positions") && !item["char_positions"].empty()) {
                        auto first_char = item["char_positions"][0];
                        if (first_char.contains("font") && first_char["font"].contains("size")) {
                            // Only consider as heading if font size > 12
                            if (first_char["font"]["size"] > 12) {
                                headings.push_back(line);
                            }
                        } else {
                            // No font info, use heuristic
                            headings.push_back(line);
                        }
                    }
                }
            }
        }
    }
    
    return headings;
}

} // namespace fast_pdf_parser