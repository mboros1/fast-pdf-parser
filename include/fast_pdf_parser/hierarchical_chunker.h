#pragma once

#include <string>
#include <vector>
#include <memory>
#include <regex>
#include <algorithm>
#include "tiktoken_tokenizer.h"

namespace fast_pdf_parser {

/**
 * Represents a semantic unit in the document hierarchy
 */
struct SemanticUnit {
    enum Type {
        HEADING1,      // # Main heading
        HEADING2,      // ## Subheading  
        HEADING3,      // ### Sub-subheading
        PARAGRAPH,     // Regular paragraph
        LIST_ITEM,     // Bullet or numbered list item
        CODE_BLOCK,    // Code snippet
        TABLE,         // Table content
        SECTION_BREAK, // Major section boundary
        PAGE_BREAK     // Page boundary
    };
    
    Type type;
    std::string text;
    int start_page;
    int end_page;
    size_t token_count;
    
    // For headings, track their level
    int heading_level = 0;
    
    // Can this unit be split if needed?
    bool splittable = true;
};

/**
 * Represents a chunk with metadata
 */
struct DocumentChunk {
    std::string text;
    std::vector<int> page_numbers;
    size_t token_count;
    
    // Semantic boundaries
    bool starts_with_heading = false;
    bool ends_cleanly = true;  // false if we had to split mid-unit
    
    // Hierarchical context (e.g., current section headings)
    std::vector<std::string> context_headings;
};

class HierarchicalChunker {
private:
    TiktokenTokenizer tokenizer_;
    size_t max_tokens_;
    size_t overlap_tokens_;
    bool merge_small_chunks_;
    
    // Patterns for detecting structure
    std::regex heading1_pattern_{"^#\\s+(.+)$"};
    std::regex heading2_pattern_{"^##\\s+(.+)$"};
    std::regex heading3_pattern_{"^###\\s+(.+)$"};
    std::regex numbered_heading_pattern_{"^\\d+(\\.\\d+)*\\s+[A-Z]"};  // "1.2.3 Title"
    std::regex bullet_pattern_{"^[•·▪▫◦‣⁃\\-\\*]\\s+"};
    std::regex numbered_list_pattern_{"^\\d+\\.\\s+"};
    std::regex code_fence_pattern_{"^```"};
    std::regex blank_line_pattern_{"^\\s*$"};
    
    /**
     * Parse text into semantic units
     */
    std::vector<SemanticUnit> parse_semantic_units(const std::vector<std::string>& page_texts,
                                                    const std::vector<int>& page_numbers) {
        std::vector<SemanticUnit> units;
        std::string current_paragraph;
        int para_start_page = 0;
        bool in_code_block = false;
        
        for (size_t page_idx = 0; page_idx < page_texts.size(); ++page_idx) {
            const auto& page_text = page_texts[page_idx];
            int page_num = page_numbers[page_idx];
            
            // Add page break marker
            if (page_idx > 0) {
                units.push_back({SemanticUnit::PAGE_BREAK, "", page_num, page_num, 0});
            }
            
            std::istringstream stream(page_text);
            std::string line;
            
            while (std::getline(stream, line)) {
                // Check for code block boundaries
                if (std::regex_match(line, code_fence_pattern_)) {
                    if (!current_paragraph.empty()) {
                        units.push_back(create_unit(current_paragraph, para_start_page, page_num, 
                                      in_code_block ? SemanticUnit::CODE_BLOCK : SemanticUnit::PARAGRAPH));
                        current_paragraph.clear();
                    }
                    in_code_block = !in_code_block;
                    continue;
                }
                
                // Skip structural analysis if in code block
                if (in_code_block) {
                    if (!current_paragraph.empty()) current_paragraph += "\n";
                    current_paragraph += line;
                    if (current_paragraph.empty()) para_start_page = page_num;
                    continue;
                }
                
                // Check for headings
                std::smatch match;
                if (std::regex_match(line, match, heading1_pattern_)) {
                    flush_paragraph(units, current_paragraph, para_start_page, page_num);
                    auto unit = create_unit(line, page_num, page_num, SemanticUnit::HEADING1);
                    unit.heading_level = 1;
                    unit.splittable = false;
                    units.push_back(unit);
                    units.push_back({SemanticUnit::SECTION_BREAK, "", page_num, page_num, 0});
                }
                else if (std::regex_match(line, match, heading2_pattern_)) {
                    flush_paragraph(units, current_paragraph, para_start_page, page_num);
                    auto unit = create_unit(line, page_num, page_num, SemanticUnit::HEADING2);
                    unit.heading_level = 2;
                    unit.splittable = false;
                    units.push_back(unit);
                }
                else if (std::regex_match(line, match, heading3_pattern_)) {
                    flush_paragraph(units, current_paragraph, para_start_page, page_num);
                    auto unit = create_unit(line, page_num, page_num, SemanticUnit::HEADING3);
                    unit.heading_level = 3;
                    unit.splittable = false;
                    units.push_back(unit);
                }
                else if (std::regex_search(line, numbered_heading_pattern_)) {
                    flush_paragraph(units, current_paragraph, para_start_page, page_num);
                    auto unit = create_unit(line, page_num, page_num, SemanticUnit::HEADING2);
                    unit.heading_level = 2;
                    unit.splittable = false;
                    units.push_back(unit);
                }
                else if (std::regex_search(line, bullet_pattern_) || 
                         std::regex_search(line, numbered_list_pattern_)) {
                    flush_paragraph(units, current_paragraph, para_start_page, page_num);
                    units.push_back(create_unit(line, page_num, page_num, SemanticUnit::LIST_ITEM));
                }
                else if (std::regex_match(line, blank_line_pattern_)) {
                    // Blank line ends paragraph
                    flush_paragraph(units, current_paragraph, para_start_page, page_num);
                }
                else {
                    // Check if this looks like a TOC entry (has dots followed by numbers)
                    if (line.find("....") != std::string::npos || 
                        line.find(". . .") != std::string::npos) {
                        // Treat each TOC line as separate unit
                        flush_paragraph(units, current_paragraph, para_start_page, page_num);
                        units.push_back(create_unit(line, page_num, page_num, SemanticUnit::LIST_ITEM));
                    } else {
                        // Continue paragraph
                        if (!current_paragraph.empty()) current_paragraph += "\n";
                        current_paragraph += line;
                        if (current_paragraph.empty()) para_start_page = page_num;
                    }
                }
            }
        }
        
        // Flush any remaining paragraph
        if (!current_paragraph.empty()) {
            flush_paragraph(units, current_paragraph, para_start_page, 
                          page_numbers.back());
        }
        
        return units;
    }
    
    void flush_paragraph(std::vector<SemanticUnit>& units, std::string& paragraph,
                        int start_page, int end_page) {
        if (!paragraph.empty()) {
            units.push_back(create_unit(paragraph, start_page, end_page, SemanticUnit::PARAGRAPH));
            paragraph.clear();
        }
    }
    
    SemanticUnit create_unit(const std::string& text, int start_page, int end_page, 
                            SemanticUnit::Type type) {
        SemanticUnit unit;
        unit.type = type;
        unit.text = text;
        unit.start_page = start_page;
        unit.end_page = end_page;
        unit.token_count = tokenizer_.count_tokens(text);
        
        // Mark very large units as splittable (except headings)
        if (unit.token_count > max_tokens_ && type != SemanticUnit::HEADING1 && 
            type != SemanticUnit::HEADING2 && type != SemanticUnit::HEADING3) {
            unit.splittable = true;
        }
        
        return unit;
    }
    
    /**
     * Merge small adjacent units of the same type
     */
    void merge_small_peers(std::vector<SemanticUnit>& units, size_t min_tokens = 50) {
        if (!merge_small_chunks_) return;
        
        std::vector<SemanticUnit> merged;
        
        for (size_t i = 0; i < units.size(); ++i) {
            if (units[i].type == SemanticUnit::PAGE_BREAK || 
                units[i].type == SemanticUnit::SECTION_BREAK ||
                units[i].type == SemanticUnit::HEADING1 ||
                units[i].type == SemanticUnit::HEADING2) {
                // Never merge these
                merged.push_back(units[i]);
                continue;
            }
            
            // Look ahead to merge small peers
            if (i + 1 < units.size() && 
                units[i].token_count < min_tokens &&
                units[i].type == units[i + 1].type &&
                units[i + 1].type != SemanticUnit::HEADING1 &&
                units[i + 1].type != SemanticUnit::HEADING2 &&
                units[i].end_page == units[i + 1].start_page) {
                
                // Merge with next
                SemanticUnit merged_unit = units[i];
                merged_unit.text += "\n" + units[i + 1].text;
                merged_unit.end_page = units[i + 1].end_page;
                merged_unit.token_count = tokenizer_.count_tokens(merged_unit.text);
                merged.push_back(merged_unit);
                i++; // Skip next unit
            } else {
                merged.push_back(units[i]);
            }
        }
        
        units = merged;
    }
    
    /**
     * Create chunks respecting semantic boundaries
     */
    std::vector<DocumentChunk> create_chunks(const std::vector<SemanticUnit>& units) {
        std::vector<DocumentChunk> chunks;
        DocumentChunk current_chunk;
        std::vector<std::string> context_stack;
        
        for (size_t i = 0; i < units.size(); ++i) {
            const auto& unit = units[i];
            
            // Update context stack for headings
            if (unit.type == SemanticUnit::HEADING1) {
                context_stack.clear();
                context_stack.push_back(unit.text);
            } else if (unit.type == SemanticUnit::HEADING2) {
                if (context_stack.size() > 1) context_stack.resize(1);
                context_stack.push_back(unit.text);
            } else if (unit.type == SemanticUnit::HEADING3) {
                if (context_stack.size() > 2) context_stack.resize(2);
                context_stack.push_back(unit.text);
            }
            
            // Skip break markers
            if (unit.type == SemanticUnit::PAGE_BREAK) {
                continue;
            }
            
            // Force new chunk on section breaks
            if (unit.type == SemanticUnit::SECTION_BREAK && !current_chunk.text.empty()) {
                finalize_chunk(chunks, current_chunk);
                current_chunk = DocumentChunk();
                current_chunk.context_headings = context_stack;
                continue;
            }
            
            // Check if adding this unit would exceed limit
            size_t combined_tokens = current_chunk.token_count + unit.token_count;
            if (!current_chunk.text.empty()) combined_tokens += 2; // for "\n\n"
            
            if (!current_chunk.text.empty() && combined_tokens > max_tokens_) {
                // Try to end chunk at good boundary
                finalize_chunk(chunks, current_chunk);
                
                // Start new chunk with overlap if configured
                current_chunk = DocumentChunk();
                current_chunk.context_headings = context_stack;
                
                // Add overlap from previous chunk if enabled
                if (overlap_tokens_ > 0 && !chunks.empty()) {
                    add_overlap(current_chunk, chunks.back());
                }
            }
            
            // Add unit to current chunk
            add_unit_to_chunk(current_chunk, unit);
            
            // Mark if this chunk starts with a heading
            if (current_chunk.text.size() == unit.text.size() && 
                (unit.type == SemanticUnit::HEADING1 || 
                 unit.type == SemanticUnit::HEADING2 ||
                 unit.type == SemanticUnit::HEADING3)) {
                current_chunk.starts_with_heading = true;
            }
        }
        
        // Finalize last chunk
        if (!current_chunk.text.empty()) {
            finalize_chunk(chunks, current_chunk);
        }
        
        return chunks;
    }
    
    void add_unit_to_chunk(DocumentChunk& chunk, const SemanticUnit& unit) {
        // If unit is too large and splittable, split it
        if (unit.splittable && unit.token_count > max_tokens_) {
            // Split by lines or sentences
            std::vector<std::string> lines;
            std::istringstream stream(unit.text);
            std::string line;
            while (std::getline(stream, line)) {
                lines.push_back(line);
            }
            
            // Add lines until we would exceed max_tokens
            std::string partial_text;
            for (const auto& line : lines) {
                std::string test_text = chunk.text;
                if (!test_text.empty()) test_text += "\n\n";
                if (!partial_text.empty()) {
                    test_text += partial_text + "\n" + line;
                } else {
                    test_text += line;
                }
                
                size_t test_tokens = tokenizer_.count_tokens(test_text);
                if (test_tokens > max_tokens_ && !chunk.text.empty()) {
                    // This line would exceed limit, stop here
                    chunk.ends_cleanly = false;
                    break;
                }
                
                if (!partial_text.empty()) partial_text += "\n";
                partial_text += line;
            }
            
            // Add what we could fit
            if (!partial_text.empty()) {
                if (!chunk.text.empty()) chunk.text += "\n\n";
                chunk.text += partial_text;
            }
        } else {
            // Add entire unit
            if (!chunk.text.empty()) chunk.text += "\n\n";
            chunk.text += unit.text;
        }
        
        // Track page numbers
        for (int page = unit.start_page; page <= unit.end_page; ++page) {
            if (std::find(chunk.page_numbers.begin(), chunk.page_numbers.end(), page) 
                == chunk.page_numbers.end()) {
                chunk.page_numbers.push_back(page);
            }
        }
        
        chunk.token_count = tokenizer_.count_tokens(chunk.text);
    }
    
    void finalize_chunk(std::vector<DocumentChunk>& chunks, DocumentChunk& chunk) {
        std::sort(chunk.page_numbers.begin(), chunk.page_numbers.end());
        chunks.push_back(chunk);
    }
    
    void add_overlap(DocumentChunk& new_chunk, const DocumentChunk& prev_chunk) {
        // Take last N tokens from previous chunk as overlap
        // This is simplified - in production, you'd want to extract actual tokens
        size_t overlap_chars = overlap_tokens_ * 4; // Rough approximation
        if (overlap_chars < prev_chunk.text.size()) {
            size_t start_pos = prev_chunk.text.size() - overlap_chars;
            // Try to start at a word boundary
            while (start_pos > 0 && prev_chunk.text[start_pos] != ' ') {
                start_pos--;
            }
            if (start_pos > 0) start_pos++; // Skip the space
            
            new_chunk.text = "[...continued from previous chunk...]\n\n" + 
                            prev_chunk.text.substr(start_pos) + "\n\n";
            new_chunk.token_count = tokenizer_.count_tokens(new_chunk.text);
        }
    }

public:
    HierarchicalChunker(size_t max_tokens = 512, 
                       size_t overlap_tokens = 0,
                       bool merge_small_chunks = true) 
        : max_tokens_(max_tokens), 
          overlap_tokens_(overlap_tokens),
          merge_small_chunks_(merge_small_chunks) {}
    
    /**
     * Chunk document hierarchically, preserving structure
     */
    std::vector<DocumentChunk> chunk_document(const std::vector<std::string>& page_texts,
                                            const std::vector<int>& page_numbers) {
        // 1. Parse into semantic units
        auto units = parse_semantic_units(page_texts, page_numbers);
        
        // 2. Merge small peer units
        merge_small_peers(units);
        
        // 3. Create chunks respecting boundaries
        return create_chunks(units);
    }
};

} // namespace fast_pdf_parser