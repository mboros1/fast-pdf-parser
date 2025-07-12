#include <fast_pdf_parser/fast_pdf_parser.h>
#include <fast_pdf_parser/tiktoken_tokenizer.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <regex>
#include <set>

namespace fs = std::filesystem;

struct Chunk {
    std::string text;
    std::set<int> pages;  // Use set to avoid duplicates
    size_t token_count = 0;
    std::vector<std::string> context_headings;
    bool starts_with_heading = false;
};

// Simple heading detection
bool is_heading(const std::string& line) {
    // Markdown-style headings
    if (line.find("# ") == 0 || line.find("## ") == 0 || line.find("### ") == 0) {
        return true;
    }
    // Numbered headings like "1.2.3 Title"
    static std::regex numbered_heading("^\\d+(\\.\\d+)*\\s+[A-Z].*");
    if (std::regex_match(line, numbered_heading)) {
        return true;
    }
    // All caps headings (common in technical docs)
    if (line.length() > 3 && line.length() < 100) {
        int upper_count = 0;
        for (char c : line) {
            if (std::isupper(c)) upper_count++;
        }
        if (upper_count > line.length() * 0.7) {
            return true;
        }
    }
    return false;
}

// Check if line is a table of contents entry
bool is_toc_entry(const std::string& line) {
    return line.find("....") != std::string::npos || 
           line.find(". . .") != std::string::npos;
}

std::vector<Chunk> create_smart_chunks(
    const std::vector<std::string>& page_texts,
    const std::vector<int>& page_numbers,
    size_t max_tokens,
    size_t overlap_tokens,
    bool merge_small_chunks) {
    
    fast_pdf_parser::TiktokenTokenizer tokenizer;
    std::vector<Chunk> chunks;
    Chunk current_chunk;
    std::vector<std::string> context_stack;
    
    // Process each page
    for (size_t page_idx = 0; page_idx < page_texts.size(); ++page_idx) {
        const auto& page_text = page_texts[page_idx];
        int page_num = page_numbers[page_idx];
        
        // Split page into lines
        std::istringstream stream(page_text);
        std::string line;
        std::string page_buffer;
        
        while (std::getline(stream, line)) {
            // Update context for headings
            if (is_heading(line)) {
                // Check if we should start a new chunk on major headings
                if ((line.find("# ") == 0 || is_heading(line)) && 
                    current_chunk.token_count > 100) {
                    // Save current chunk
                    if (!current_chunk.text.empty() && !current_chunk.pages.empty()) {
                        chunks.push_back(current_chunk);
                    }
                    // Start new chunk
                    current_chunk = Chunk();
                    current_chunk.starts_with_heading = true;
                    current_chunk.context_headings = context_stack;
                    
                    // Add overlap if configured
                    if (overlap_tokens > 0 && !chunks.empty()) {
                        const auto& prev = chunks.back();
                        size_t overlap_chars = overlap_tokens * 4;
                        if (overlap_chars < prev.text.size()) {
                            size_t start = prev.text.size() - overlap_chars;
                            // Find word boundary
                            while (start > 0 && prev.text[start] != ' ') start--;
                            if (start > 0) {
                                current_chunk.text = "[...] " + prev.text.substr(start) + "\n\n";
                                current_chunk.token_count = tokenizer.count_tokens(current_chunk.text);
                                // Don't inherit all pages - overlap is just context
                            }
                        }
                    }
                }
                
                // Update context
                if (line.find("# ") == 0) {
                    context_stack.clear();
                    context_stack.push_back(line);
                } else if (line.find("## ") == 0) {
                    if (context_stack.size() > 1) context_stack.resize(1);
                    context_stack.push_back(line);
                } else if (line.find("### ") == 0) {
                    if (context_stack.size() > 2) context_stack.resize(2);
                    context_stack.push_back(line);
                }
            }
            
            // For TOC entries, add each line separately
            if (is_toc_entry(line)) {
                if (!page_buffer.empty()) page_buffer += "\n";
                page_buffer += line;
            } else {
                // Regular content
                if (!page_buffer.empty()) page_buffer += "\n";
                page_buffer += line;
            }
        }
        
        // Check if adding this page would exceed limit
        size_t page_tokens = tokenizer.count_tokens(page_buffer);
        size_t potential_tokens = current_chunk.token_count + page_tokens;
        if (!current_chunk.text.empty()) potential_tokens += 2; // for "\n\n"
        
        if (!current_chunk.text.empty() && potential_tokens > max_tokens) {
            // Save current chunk
            if (!current_chunk.pages.empty()) {
                chunks.push_back(current_chunk);
            }
            
            // Start new chunk
            current_chunk = Chunk();
            current_chunk.context_headings = context_stack;
            
            // Add overlap
            if (overlap_tokens > 0 && !chunks.empty()) {
                const auto& prev = chunks.back();
                size_t overlap_chars = overlap_tokens * 4;
                if (overlap_chars < prev.text.size()) {
                    size_t start = prev.text.size() - overlap_chars;
                    while (start > 0 && prev.text[start] != ' ') start--;
                    if (start > 0) {
                        current_chunk.text = "[...] " + prev.text.substr(start) + "\n\n";
                        current_chunk.token_count = tokenizer.count_tokens(current_chunk.text);
                        // Copy page numbers from overlap source
                        current_chunk.pages = prev.pages;
                    }
                }
            }
        }
        
        // Check if page itself exceeds max tokens and needs splitting
        if (page_tokens > max_tokens) {
            // Split the page by lines
            std::istringstream page_stream(page_buffer);
            std::string line;
            std::vector<std::string> lines;
            
            // Collect all lines
            while (std::getline(page_stream, line)) {
                lines.push_back(line);
            }
            
            // Process lines
            for (const auto& line : lines) {
                size_t line_tokens = tokenizer.count_tokens(line);
                
                // Check if adding this line would exceed the limit
                size_t potential_tokens = current_chunk.token_count + line_tokens;
                if (!current_chunk.text.empty()) potential_tokens += 2; // for newline
                
                if (potential_tokens > max_tokens && !current_chunk.text.empty()) {
                    // Save current chunk
                    if (!current_chunk.pages.empty()) {
                        chunks.push_back(current_chunk);
                    }
                    
                    // Start new chunk
                    current_chunk = Chunk();
                    current_chunk.context_headings = context_stack;
                    
                    // Add overlap if configured
                    if (overlap_tokens > 0 && !chunks.empty()) {
                        const auto& prev = chunks.back();
                        size_t overlap_chars = overlap_tokens * 4;
                        if (overlap_chars < prev.text.size()) {
                            size_t start = prev.text.size() - overlap_chars;
                            while (start > 0 && prev.text[start] != ' ') start--;
                            if (start > 0) {
                                current_chunk.text = "[...] " + prev.text.substr(start) + "\n\n";
                                current_chunk.token_count = tokenizer.count_tokens(current_chunk.text);
                                current_chunk.pages = prev.pages;
                            }
                        }
                    }
                }
                
                // Add line to current chunk
                if (!current_chunk.text.empty()) {
                    current_chunk.text += "\n";
                }
                current_chunk.text += line;
                current_chunk.pages.insert(page_num);
                current_chunk.token_count = tokenizer.count_tokens(current_chunk.text);
                
                // If this single line exceeds max_tokens, we still need to add it
                // but immediately start a new chunk afterwards
                if (current_chunk.token_count > max_tokens) {
                    if (!current_chunk.pages.empty()) {
                        chunks.push_back(current_chunk);
                    }
                    current_chunk = Chunk();
                    current_chunk.context_headings = context_stack;
                }
            }
        } else {
            // Page fits within limit, add it normally
            if (!current_chunk.text.empty()) current_chunk.text += "\n\n";
            current_chunk.text += page_buffer;
            current_chunk.pages.insert(page_num);
            current_chunk.token_count = tokenizer.count_tokens(current_chunk.text);
        }
    }
    
    // Save final chunk
    if (!current_chunk.text.empty() && !current_chunk.pages.empty()) {
        chunks.push_back(current_chunk);
    }
    
    // Merge small chunks if requested
    if (merge_small_chunks && chunks.size() > 1) {
        std::vector<Chunk> merged_chunks;
        
        for (size_t i = 0; i < chunks.size(); ++i) {
            if (chunks[i].token_count < 100 && i + 1 < chunks.size() && 
                chunks[i].token_count + chunks[i + 1].token_count < max_tokens) {
                // Merge with next chunk
                chunks[i + 1].text = chunks[i].text + "\n\n" + chunks[i + 1].text;
                chunks[i + 1].pages.insert(chunks[i].pages.begin(), chunks[i].pages.end());
                chunks[i + 1].token_count = tokenizer.count_tokens(chunks[i + 1].text);
                if (chunks[i].starts_with_heading) {
                    chunks[i + 1].starts_with_heading = true;
                }
            } else {
                merged_chunks.push_back(chunks[i]);
            }
        }
        
        chunks = merged_chunks;
    }
    
    return chunks;
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 4) {
        std::cerr << "Usage: " << argv[0] << " <input.pdf> [max_tokens=512] [overlap_tokens=50]\n";
        return 1;
    }

    size_t max_tokens = 512;
    size_t overlap_tokens = 50;
    
    if (argc >= 3) max_tokens = std::stoul(argv[2]);
    if (argc >= 4) overlap_tokens = std::stoul(argv[3]);

    try {
        fast_pdf_parser::ParseOptions options;
        options.thread_count = std::max(1u, std::thread::hardware_concurrency() - 1);
        options.batch_size = 10;
        options.extract_positions = false;
        options.extract_fonts = false;
        
        fast_pdf_parser::FastPdfParser parser(options);
        
        std::cout << "Processing: " << argv[1] << " with " << options.thread_count << " threads\n";
        std::cout << "Chunking: max_tokens=" << max_tokens << ", overlap=" << overlap_tokens << "\n";
        auto start = std::chrono::high_resolution_clock::now();
        
        // Create output directory
        fs::create_directories("./out");
        
        // Collect all pages
        std::vector<std::string> page_texts;
        std::vector<int> page_numbers;
        
        parser.parse_streaming(argv[1], [&](fast_pdf_parser::PageResult result) -> bool {
            if (result.success) {
                std::string page_text;
                if (result.content.contains("blocks")) {
                    for (const auto& block : result.content["blocks"]) {
                        for (const auto& line : block["lines"]) {
                            if (!page_text.empty()) page_text += "\n";
                            page_text += line["text"].get<std::string>();
                        }
                    }
                }
                page_texts.push_back(page_text);
                page_numbers.push_back(result.page_number);
            }
            return true;
        });
        
        std::cout << "Extracted " << page_texts.size() << " pages, creating smart chunks...\n";
        
        // Create chunks
        auto chunks = create_smart_chunks(page_texts, page_numbers, max_tokens, overlap_tokens, true);
        
        // Calculate file hash
        std::hash<std::string> hasher;
        int64_t file_hash = static_cast<int64_t>(hasher(argv[1]));
        
        // Write output
        std::string pdf_name = fs::path(argv[1]).stem().string();
        std::ofstream outfile("./out/" + pdf_name + "_smart_chunks.json");
        
        outfile << "[\n";
        
        for (size_t i = 0; i < chunks.size(); ++i) {
            const auto& chunk = chunks[i];
            
            if (i > 0) outfile << ",\n";
            
            rapidjson::Document chunk_doc;
            chunk_doc.SetObject();
            auto& alloc = chunk_doc.GetAllocator();
            
            // Text
            rapidjson::Value text_val;
            text_val.SetString(chunk.text.c_str(), chunk.text.length(), alloc);
            chunk_doc.AddMember("text", text_val, alloc);
            
            // Metadata
            rapidjson::Value meta;
            meta.SetObject();
            
            meta.AddMember("schema_name", "docling_core.transforms.chunker.DocMeta", alloc);
            meta.AddMember("version", "1.0.0", alloc);
            
            // Page numbers
            rapidjson::Value pages(rapidjson::kArrayType);
            for (int page : chunk.pages) {
                pages.PushBack(page, alloc);
            }
            meta.AddMember("page_numbers", pages, alloc);
            meta.AddMember("page_count", static_cast<int>(chunk.pages.size()), alloc);
            
            // Chunk info
            meta.AddMember("chunk_index", static_cast<int>(i), alloc);
            meta.AddMember("total_chunks", static_cast<int>(chunks.size()), alloc);
            meta.AddMember("token_count", static_cast<int>(chunk.token_count), alloc);
            meta.AddMember("starts_with_heading", chunk.starts_with_heading, alloc);
            
            // Context
            if (!chunk.context_headings.empty()) {
                rapidjson::Value context(rapidjson::kArrayType);
                for (const auto& heading : chunk.context_headings) {
                    rapidjson::Value h;
                    h.SetString(heading.c_str(), heading.length(), alloc);
                    context.PushBack(h, alloc);
                }
                meta.AddMember("context_headings", context, alloc);
            }
            
            // Origin
            rapidjson::Value origin;
            origin.SetObject();
            origin.AddMember("mimetype", "application/pdf", alloc);
            origin.AddMember("binary_hash", file_hash, alloc);
            
            rapidjson::Value filename;
            filename.SetString(fs::path(argv[1]).filename().string().c_str(), alloc);
            origin.AddMember("filename", filename, alloc);
            origin.AddMember("uri", rapidjson::Value().SetNull(), alloc);
            
            meta.AddMember("origin", origin, alloc);
            meta.AddMember("doc_items", rapidjson::Value(rapidjson::kArrayType), alloc);
            meta.AddMember("headings", rapidjson::Value(rapidjson::kArrayType), alloc);
            meta.AddMember("captions", rapidjson::Value().SetNull(), alloc);
            
            chunk_doc.AddMember("meta", meta, alloc);
            
            // Write
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            chunk_doc.Accept(writer);
            outfile << buffer.GetString();
        }
        
        outfile << "\n]\n";
        outfile.close();
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "\nResults:\n";
        std::cout << "Created " << chunks.size() << " chunks from " << page_texts.size() << " pages\n";
        std::cout << "Total time: " << duration.count() << "ms\n";
        std::cout << "Performance: " << (page_texts.size() * 1000.0) / duration.count() << " pages/second\n";
        std::cout << "Output: ./out/" << pdf_name << "_smart_chunks.json\n";
        
        // Statistics
        size_t min_tokens = chunks[0].token_count;
        size_t max_tokens = chunks[0].token_count;
        size_t total_tokens = 0;
        size_t min_pages = chunks[0].pages.size();
        size_t max_pages = chunks[0].pages.size();
        int empty_page_chunks = 0;
        
        for (const auto& chunk : chunks) {
            min_tokens = std::min(min_tokens, chunk.token_count);
            max_tokens = std::max(max_tokens, chunk.token_count);
            total_tokens += chunk.token_count;
            min_pages = std::min(min_pages, chunk.pages.size());
            max_pages = std::max(max_pages, chunk.pages.size());
            if (chunk.pages.empty()) empty_page_chunks++;
        }
        
        std::cout << "\nChunk statistics:\n";
        std::cout << "  Token range: " << min_tokens << "-" << max_tokens 
                  << " (avg: " << total_tokens / chunks.size() << ")\n";
        std::cout << "  Pages per chunk: " << min_pages << "-" << max_pages << "\n";
        if (empty_page_chunks > 0) {
            std::cout << "  WARNING: " << empty_page_chunks << " chunks have no page numbers!\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}