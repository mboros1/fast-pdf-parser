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
#include <algorithm>
#include <numeric>

namespace fs = std::filesystem;

// Enhanced chunk structure with hierarchical information
struct Chunk {
    std::string text;
    int start_page = -1;
    int end_page = -1;
    size_t token_count = 0;
    std::vector<std::string> context_headings;
    bool starts_with_heading = false;
    int heading_level = 0;  // 0 = no heading, 1 = #, 2 = ##, 3 = ###
    bool contains_major_heading = false;  // Contains a # heading
};

// Copy all the helper structures and functions from hierarchical_refactored.cpp
struct AnnotatedLine {
    std::string text;
    int page_number;
    bool is_heading = false;
    bool is_major_heading = false;
    bool is_toc_entry = false;
    int heading_level = 0;
};

struct SemanticUnit {
    std::vector<AnnotatedLine> lines;
    int start_page;
    int end_page;
    size_t token_count = 0;
    bool is_heading_unit = false;
    int heading_level = 0;
};

// Helper functions (same as before)
bool is_markdown_heading(const std::string& line) {
    return line.find("# ") == 0 || line.find("## ") == 0 || line.find("### ") == 0;
}

int get_heading_level(const std::string& line) {
    if (line.find("### ") == 0) return 3;
    if (line.find("## ") == 0) return 2;
    if (line.find("# ") == 0) return 1;
    return 0;
}

bool is_numbered_heading(const std::string& line) {
    static std::regex pattern("^\\d+(\\.\\d+)*\\s+[A-Z].*");
    return std::regex_match(line, pattern);
}

bool is_caps_heading(const std::string& line) {
    if (line.length() < 3 || line.length() > 100) return false;
    int upper_count = std::count_if(line.begin(), line.end(), ::isupper);
    return upper_count > line.length() * 0.7;
}

bool is_toc_entry(const std::string& line) {
    return line.find("....") != std::string::npos || 
           line.find(". . .") != std::string::npos;
}

// Enhanced Pass 5: Aggressive merging for undersized chunks
std::vector<Chunk> merge_small_chunks_hierarchically(
    const std::vector<Chunk>& chunks,
    size_t min_tokens,
    size_t max_tokens,
    fast_pdf_parser::TiktokenTokenizer& tokenizer) {
    
    if (chunks.empty()) return chunks;
    
    // First pass: Standard hierarchical merging
    std::vector<Chunk> first_pass;
    Chunk accumulator = chunks[0];
    
    for (size_t i = 1; i < chunks.size(); ++i) {
        const auto& next = chunks[i];
        
        // Determine if we should merge based on hierarchical rules
        bool should_merge = false;
        
        // Never merge across major headings (# level) unless both chunks are tiny
        if (next.starts_with_heading && next.heading_level == 1 && 
            accumulator.token_count >= min_tokens) {
            should_merge = false;
        }
        // Always try to merge if current chunk is undersized
        else if (accumulator.token_count < min_tokens) {
            // Check if merging would exceed token limit
            if (accumulator.token_count + next.token_count <= max_tokens) {
                // Very aggressive merging for small chunks
                should_merge = true;
                
                // Only respect ## headings if we're already reasonably sized
                if (next.starts_with_heading && next.heading_level == 2 && 
                    accumulator.token_count > min_tokens * 0.75) {
                    should_merge = false;
                }
            }
        }
        // Also merge if next chunk is very small
        else if (next.token_count < min_tokens && 
                 accumulator.token_count + next.token_count <= max_tokens &&
                 !next.contains_major_heading) {
            should_merge = true;
        }
        
        if (should_merge) {
            // Merge chunks
            accumulator.text += "\n\n" + next.text;
            accumulator.end_page = next.end_page;
            accumulator.token_count = tokenizer.count_tokens(accumulator.text);
            
            // Update heading information
            if (!accumulator.starts_with_heading && next.starts_with_heading) {
                accumulator.starts_with_heading = true;
                accumulator.heading_level = next.heading_level;
            }
            if (next.contains_major_heading) {
                accumulator.contains_major_heading = true;
            }
            
            // Merge context headings
            if (accumulator.context_headings != next.context_headings) {
                // Keep the context that represents the start of the merged chunk
                if (accumulator.context_headings.empty()) {
                    accumulator.context_headings = next.context_headings;
                }
            }
        } else {
            // Save current accumulator and start new one
            first_pass.push_back(accumulator);
            accumulator = next;
        }
    }
    
    // Don't forget the last chunk
    first_pass.push_back(accumulator);
    
    // Second pass: Merge any remaining undersized chunks more aggressively
    std::vector<Chunk> final_pass;
    accumulator = first_pass[0];
    
    for (size_t i = 1; i < first_pass.size(); ++i) {
        const auto& next = first_pass[i];
        
        // If current chunk is still undersized, try harder to merge
        bool force_merge = false;
        
        // If accumulator is undersized and can fit next chunk
        if (accumulator.token_count < min_tokens && 
            accumulator.token_count + next.token_count <= max_tokens) {
            // Only respect major headings if the next chunk is reasonably sized
            if (next.contains_major_heading && next.token_count >= min_tokens) {
                force_merge = false;
            } else {
                force_merge = true;
            }
        }
        // If next chunk is undersized and can fit with accumulator
        else if (next.token_count < min_tokens && 
                 accumulator.token_count + next.token_count <= max_tokens &&
                 !next.contains_major_heading) {
            force_merge = true;
        }
        
        if (force_merge) {
            // Merge chunks
            accumulator.text += "\n\n" + next.text;
            accumulator.end_page = next.end_page;
            accumulator.token_count = tokenizer.count_tokens(accumulator.text);
            
            // Update metadata
            if (!accumulator.starts_with_heading && next.starts_with_heading) {
                accumulator.starts_with_heading = true;
                accumulator.heading_level = next.heading_level;
            }
            if (next.contains_major_heading) {
                accumulator.contains_major_heading = true;
            }
        } else {
            final_pass.push_back(accumulator);
            accumulator = next;
        }
    }
    
    final_pass.push_back(accumulator);
    
    return final_pass;
}

// Enhanced Pass 3: Create chunks with heading level tracking
std::vector<Chunk> create_chunks_from_units_enhanced(
    const std::vector<SemanticUnit>& units,
    size_t max_tokens,
    fast_pdf_parser::TiktokenTokenizer& tokenizer) {
    
    std::vector<Chunk> chunks;
    Chunk current_chunk;
    std::vector<std::string> context_stack;
    
    for (const auto& unit : units) {
        // Update context stack for heading units
        if (unit.is_heading_unit && !unit.lines.empty()) {
            const auto& first_line = unit.lines[0];
            if (first_line.heading_level == 1) {
                context_stack.clear();
                context_stack.push_back(first_line.text);
            } else if (first_line.heading_level == 2) {
                if (context_stack.size() > 1) context_stack.resize(1);
                context_stack.push_back(first_line.text);
            } else if (first_line.heading_level == 3) {
                if (context_stack.size() > 2) context_stack.resize(2);
                context_stack.push_back(first_line.text);
            }
        }
        
        // Check if this unit needs to be split
        if (unit.token_count > max_tokens) {
            // Process lines individually (same as before)
            for (const auto& line : unit.lines) {
                std::string line_text = line.text;
                size_t line_tokens = tokenizer.count_tokens(line_text);
                
                size_t potential_tokens = current_chunk.token_count + line_tokens;
                if (!current_chunk.text.empty()) potential_tokens += 1;
                
                if (!current_chunk.text.empty() && potential_tokens > max_tokens) {
                    chunks.push_back(current_chunk);
                    current_chunk = Chunk();
                    current_chunk.context_headings = context_stack;
                    current_chunk.start_page = line.page_number;
                }
                
                if (current_chunk.text.empty()) {
                    current_chunk.start_page = line.page_number;
                    current_chunk.starts_with_heading = line.is_heading;
                    current_chunk.heading_level = line.heading_level;
                    current_chunk.context_headings = context_stack;
                    if (line.heading_level == 1) {
                        current_chunk.contains_major_heading = true;
                    }
                }
                
                if (!current_chunk.text.empty()) current_chunk.text += "\n";
                current_chunk.text += line_text;
                current_chunk.end_page = line.page_number;
                current_chunk.token_count = tokenizer.count_tokens(current_chunk.text);
                
                if (current_chunk.token_count > max_tokens) {
                    chunks.push_back(current_chunk);
                    current_chunk = Chunk();
                    current_chunk.context_headings = context_stack;
                }
            }
        } else {
            // Unit fits within token limit
            std::string unit_text;
            for (const auto& line : unit.lines) {
                if (!unit_text.empty()) unit_text += "\n";
                unit_text += line.text;
            }
            
            size_t potential_tokens = current_chunk.token_count + unit.token_count;
            if (!current_chunk.text.empty()) potential_tokens += 2;
            
            if (!current_chunk.text.empty() && potential_tokens > max_tokens) {
                chunks.push_back(current_chunk);
                current_chunk = Chunk();
                current_chunk.context_headings = context_stack;
            }
            
            if (current_chunk.text.empty()) {
                current_chunk.start_page = unit.start_page;
                current_chunk.starts_with_heading = unit.is_heading_unit;
                current_chunk.heading_level = unit.heading_level;
                current_chunk.context_headings = context_stack;
            }
            
            if (!current_chunk.text.empty()) current_chunk.text += "\n\n";
            current_chunk.text += unit_text;
            current_chunk.end_page = unit.end_page;
            current_chunk.token_count = tokenizer.count_tokens(current_chunk.text);
            
            // Track if we added a major heading
            if (unit.is_heading_unit && unit.heading_level == 1) {
                current_chunk.contains_major_heading = true;
            }
        }
    }
    
    if (!current_chunk.text.empty()) {
        chunks.push_back(current_chunk);
    }
    
    return chunks;
}

// Copy other necessary functions from hierarchical_refactored.cpp
std::vector<AnnotatedLine> annotate_lines(
    const std::vector<std::string>& page_texts,
    const std::vector<int>& page_numbers) {
    
    std::vector<AnnotatedLine> annotated_lines;
    
    for (size_t i = 0; i < page_texts.size(); ++i) {
        std::istringstream stream(page_texts[i]);
        std::string line;
        
        while (std::getline(stream, line)) {
            AnnotatedLine al;
            al.text = line;
            al.page_number = page_numbers[i];
            
            if (is_markdown_heading(line)) {
                al.is_heading = true;
                al.heading_level = get_heading_level(line);
                al.is_major_heading = (al.heading_level == 1);
            } else if (is_numbered_heading(line) || is_caps_heading(line)) {
                al.is_heading = true;
                al.heading_level = 2;
            }
            
            al.is_toc_entry = is_toc_entry(line);
            annotated_lines.push_back(al);
        }
    }
    
    return annotated_lines;
}

std::vector<SemanticUnit> create_semantic_units(
    const std::vector<AnnotatedLine>& lines,
    fast_pdf_parser::TiktokenTokenizer& tokenizer) {
    
    std::vector<SemanticUnit> units;
    SemanticUnit current_unit;
    
    for (const auto& line : lines) {
        if (line.is_major_heading && !current_unit.lines.empty()) {
            if (!current_unit.lines.empty()) {
                current_unit.end_page = current_unit.lines.back().page_number;
                std::string unit_text;
                for (const auto& l : current_unit.lines) {
                    if (!unit_text.empty()) unit_text += "\n";
                    unit_text += l.text;
                }
                current_unit.token_count = tokenizer.count_tokens(unit_text);
                units.push_back(current_unit);
            }
            current_unit = SemanticUnit();
        }
        
        if (current_unit.lines.empty()) {
            current_unit.start_page = line.page_number;
            current_unit.is_heading_unit = line.is_heading;
            current_unit.heading_level = line.heading_level;
        }
        current_unit.lines.push_back(line);
        
        if (line.text.empty() || 
            (lines.size() > 1 && &line != &lines.back() && 
             line.page_number != (&line + 1)->page_number)) {
            if (!current_unit.lines.empty()) {
                current_unit.end_page = line.page_number;
                std::string unit_text;
                for (const auto& l : current_unit.lines) {
                    if (!unit_text.empty()) unit_text += "\n";
                    unit_text += l.text;
                }
                current_unit.token_count = tokenizer.count_tokens(unit_text);
                units.push_back(current_unit);
                current_unit = SemanticUnit();
            }
        }
    }
    
    if (!current_unit.lines.empty()) {
        current_unit.end_page = current_unit.lines.back().page_number;
        std::string unit_text;
        for (const auto& l : current_unit.lines) {
            if (!unit_text.empty()) unit_text += "\n";
            unit_text += l.text;
        }
        current_unit.token_count = tokenizer.count_tokens(unit_text);
        units.push_back(current_unit);
    }
    
    return units;
}

// Copy remaining functions...
void add_overlap_to_chunks(std::vector<Chunk>& chunks, size_t overlap_tokens) {
    if (overlap_tokens == 0 || chunks.size() < 2) return;
    
    for (size_t i = 1; i < chunks.size(); ++i) {
        const auto& prev = chunks[i-1];
        auto& current = chunks[i];
        
        size_t overlap_chars = overlap_tokens * 4;
        if (overlap_chars < prev.text.size()) {
            size_t start = prev.text.size() - overlap_chars;
            while (start > 0 && prev.text[start] != ' ') start--;
            if (start > 0) {
                std::string overlap = "[...] " + prev.text.substr(start);
                current.text = overlap + "\n\n" + current.text;
            }
        }
    }
}

std::vector<Chunk> split_oversized_chunks(
    const std::vector<Chunk>& chunks,
    size_t max_tokens,
    fast_pdf_parser::TiktokenTokenizer& tokenizer) {
    
    std::vector<Chunk> result;
    
    for (const auto& chunk : chunks) {
        size_t actual_tokens = tokenizer.count_tokens(chunk.text);
        if (actual_tokens <= max_tokens) {
            result.push_back(chunk);
            continue;
        }
        
        std::vector<std::pair<size_t, std::string>> segments;
        std::string current_segment;
        size_t last_pos = 0;
        
        size_t pos = 0;
        while ((pos = chunk.text.find("\n\n", last_pos)) != std::string::npos) {
            current_segment = chunk.text.substr(last_pos, pos - last_pos);
            if (!current_segment.empty()) {
                segments.push_back({last_pos, current_segment});
            }
            last_pos = pos + 2;
        }
        
        if (last_pos < chunk.text.length()) {
            current_segment = chunk.text.substr(last_pos);
            if (!current_segment.empty()) {
                segments.push_back({last_pos, current_segment});
            }
        }
        
        if (segments.size() <= 1) {
            segments.clear();
            last_pos = 0;
            pos = 0;
            while ((pos = chunk.text.find("\n", last_pos)) != std::string::npos) {
                current_segment = chunk.text.substr(last_pos, pos - last_pos);
                if (!current_segment.empty()) {
                    segments.push_back({last_pos, current_segment});
                }
                last_pos = pos + 1;
            }
            if (last_pos < chunk.text.length()) {
                current_segment = chunk.text.substr(last_pos);
                if (!current_segment.empty()) {
                    segments.push_back({last_pos, current_segment});
                }
            }
        }
        
        if (segments.size() <= 1) {
            segments.clear();
            std::regex sentence_end("[.!?]\\s+");
            std::sregex_iterator it(chunk.text.begin(), chunk.text.end(), sentence_end);
            std::sregex_iterator end;
            
            last_pos = 0;
            for (; it != end; ++it) {
                size_t match_pos = it->position() + it->length();
                current_segment = chunk.text.substr(last_pos, match_pos - last_pos);
                if (!current_segment.empty()) {
                    segments.push_back({last_pos, current_segment});
                }
                last_pos = match_pos;
            }
            if (last_pos < chunk.text.length()) {
                current_segment = chunk.text.substr(last_pos);
                if (!current_segment.empty()) {
                    segments.push_back({last_pos, current_segment});
                }
            }
        }
        
        Chunk new_chunk = chunk;
        new_chunk.text.clear();
        new_chunk.token_count = 0;
        new_chunk.contains_major_heading = false;
        
        for (const auto& [seg_pos, seg_text] : segments) {
            size_t seg_tokens = tokenizer.count_tokens(seg_text);
            size_t potential_tokens = new_chunk.token_count + seg_tokens;
            
            if (!new_chunk.text.empty()) {
                if (seg_pos > 0 && seg_pos > 1 && chunk.text[seg_pos-1] == '\n' && chunk.text[seg_pos-2] == '\n') {
                    potential_tokens += 2;
                } else if (seg_pos > 0 && chunk.text[seg_pos-1] == '\n') {
                    potential_tokens += 1;
                } else {
                    potential_tokens += 1;
                }
            }
            
            if (!new_chunk.text.empty() && potential_tokens > max_tokens) {
                new_chunk.token_count = tokenizer.count_tokens(new_chunk.text);
                result.push_back(new_chunk);
                
                new_chunk = chunk;
                new_chunk.text = seg_text;
                new_chunk.token_count = seg_tokens;
                new_chunk.contains_major_heading = false;
                new_chunk.starts_with_heading = false;
            } else {
                if (!new_chunk.text.empty()) {
                    if (seg_pos > 0 && seg_pos > 1 && chunk.text[seg_pos-1] == '\n' && chunk.text[seg_pos-2] == '\n') {
                        new_chunk.text += "\n\n";
                    } else if (seg_pos > 0 && chunk.text[seg_pos-1] == '\n') {
                        new_chunk.text += "\n";
                    } else if (!new_chunk.text.empty() && new_chunk.text.back() != ' ') {
                        new_chunk.text += " ";
                    }
                }
                new_chunk.text += seg_text;
                new_chunk.token_count = tokenizer.count_tokens(new_chunk.text);
            }
        }
        
        if (!new_chunk.text.empty()) {
            new_chunk.token_count = tokenizer.count_tokens(new_chunk.text);
            result.push_back(new_chunk);
        }
    }
    
    return result;
}

// Main chunking function
std::vector<Chunk> create_hierarchical_chunks_enhanced(
    const std::vector<std::string>& page_texts,
    const std::vector<int>& page_numbers,
    size_t max_tokens,
    size_t overlap_tokens,
    bool should_merge_small_chunks) {
    
    fast_pdf_parser::TiktokenTokenizer tokenizer;
    
    auto annotated_lines = annotate_lines(page_texts, page_numbers);
    auto semantic_units = create_semantic_units(annotated_lines, tokenizer);
    auto chunks = create_chunks_from_units_enhanced(semantic_units, max_tokens, tokenizer);
    
    add_overlap_to_chunks(chunks, overlap_tokens);
    
    if (should_merge_small_chunks) {
        // Use higher minimum (150 tokens) to aggressively merge small chunks
        chunks = merge_small_chunks_hierarchically(chunks, 150, max_tokens, tokenizer);
    }
    
    chunks = split_oversized_chunks(chunks, max_tokens, tokenizer);
    
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
        std::cout << "Enhanced hierarchical chunking: max_tokens=" << max_tokens << ", overlap=" << overlap_tokens << "\n";
        auto start = std::chrono::high_resolution_clock::now();
        
        fs::create_directories("./out");
        
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
        
        std::cout << "Extracted " << page_texts.size() << " pages, creating enhanced hierarchical chunks...\n";
        
        auto chunks = create_hierarchical_chunks_enhanced(page_texts, page_numbers, max_tokens, overlap_tokens, true);
        
        // Output generation (same as before)
        std::hash<std::string> hasher;
        int64_t file_hash = static_cast<int64_t>(hasher(argv[1]));
        
        std::string pdf_name = fs::path(argv[1]).stem().string();
        std::ofstream outfile("./out/" + pdf_name + "_enhanced_chunks.json");
        
        outfile << "[\n";
        
        fast_pdf_parser::TiktokenTokenizer tokenizer;
        for (size_t i = 0; i < chunks.size(); ++i) {
            const auto& chunk = chunks[i];
            
            if (i > 0) outfile << ",\n";
            
            rapidjson::Document chunk_doc;
            chunk_doc.SetObject();
            auto& alloc = chunk_doc.GetAllocator();
            
            rapidjson::Value text_val;
            text_val.SetString(chunk.text.c_str(), chunk.text.length(), alloc);
            chunk_doc.AddMember("text", text_val, alloc);
            
            rapidjson::Value meta;
            meta.SetObject();
            
            meta.AddMember("schema_name", "docling_core.transforms.chunker.DocMeta", alloc);
            meta.AddMember("version", "1.0.0", alloc);
            meta.AddMember("start_page", chunk.start_page, alloc);
            meta.AddMember("end_page", chunk.end_page, alloc);
            meta.AddMember("page_count", chunk.end_page - chunk.start_page + 1, alloc);
            meta.AddMember("chunk_index", static_cast<int>(i), alloc);
            meta.AddMember("total_chunks", static_cast<int>(chunks.size()), alloc);
            
            size_t actual_tokens = tokenizer.count_tokens(chunk.text);
            meta.AddMember("token_count", static_cast<int>(actual_tokens), alloc);
            meta.AddMember("starts_with_heading", chunk.starts_with_heading, alloc);
            meta.AddMember("heading_level", chunk.heading_level, alloc);
            meta.AddMember("contains_major_heading", chunk.contains_major_heading, alloc);
            
            if (!chunk.context_headings.empty()) {
                rapidjson::Value context(rapidjson::kArrayType);
                for (const auto& heading : chunk.context_headings) {
                    rapidjson::Value h;
                    h.SetString(heading.c_str(), heading.length(), alloc);
                    context.PushBack(h, alloc);
                }
                meta.AddMember("context_headings", context, alloc);
            }
            
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
        std::cout << "Output: ./out/" << pdf_name << "_enhanced_chunks.json\n";
        
        // Enhanced statistics
        if (!chunks.empty()) {
            size_t min_tokens = std::numeric_limits<size_t>::max();
            size_t max_tokens = 0;
            size_t total_tokens = 0;
            int min_pages = std::numeric_limits<int>::max();
            int max_pages = 0;
            int chunks_with_major_headings = 0;
            int chunks_starting_with_headings = 0;
            
            for (const auto& chunk : chunks) {
                size_t tokens = tokenizer.count_tokens(chunk.text);
                int pages = chunk.end_page - chunk.start_page + 1;
                
                min_tokens = std::min(min_tokens, tokens);
                max_tokens = std::max(max_tokens, tokens);
                total_tokens += tokens;
                min_pages = std::min(min_pages, pages);
                max_pages = std::max(max_pages, pages);
                
                if (chunk.contains_major_heading) chunks_with_major_headings++;
                if (chunk.starts_with_heading) chunks_starting_with_headings++;
            }
            
            std::cout << "\nChunk statistics:\n";
            std::cout << "  Token range: " << min_tokens << "-" << max_tokens 
                      << " (avg: " << total_tokens / chunks.size() << ")\n";
            std::cout << "  Pages per chunk: " << min_pages << "-" << max_pages << "\n";
            std::cout << "  Chunks with major headings: " << chunks_with_major_headings << "\n";
            std::cout << "  Chunks starting with headings: " << chunks_starting_with_headings << "\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}