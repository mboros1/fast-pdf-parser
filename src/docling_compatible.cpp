#include <fast_pdf_parser/fast_pdf_parser.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <fast_pdf_parser/tiktoken_tokenizer.h>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input.pdf>\n";
        return 1;
    }

    try {
        // Initialize tokenizer
        fast_pdf_parser::TiktokenTokenizer tokenizer;
        
        fast_pdf_parser::ParseOptions options;
        options.thread_count = std::max(1u, std::thread::hardware_concurrency() - 1);
        options.batch_size = 10;
        options.extract_positions = false;
        options.extract_fonts = false;
        
        fast_pdf_parser::FastPdfParser parser(options);
        
        std::cout << "Processing: " << argv[1] << " with " << options.thread_count << " threads\n";
        auto start = std::chrono::high_resolution_clock::now();
        
        // Create output directory
        fs::create_directories("./out");
        
        // Collect all pages first
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
        
        // Now create chunks similar to Docling's HybridChunker
        std::string pdf_name = fs::path(argv[1]).stem().string();
        std::ofstream outfile("./out/" + pdf_name + "_chunks.json");
        
        // Calculate file hash (simple for demo)
        std::hash<std::string> hasher;
        int64_t file_hash = static_cast<int64_t>(hasher(argv[1]));
        
        // Start JSON array
        outfile << "[\n";
        
        // Create chunks (simple approach: max 512 tokens per chunk)
        std::string current_chunk;
        std::vector<int> current_pages;
        size_t chunk_count = 0;
        bool first_chunk = true;
        
        for (size_t i = 0; i < page_texts.size(); ++i) {
            const auto& page_text = page_texts[i];
            size_t page_tokens = tokenizer.count_tokens(page_text);
            size_t current_chunk_tokens = tokenizer.count_tokens(current_chunk);
            
            if (!current_chunk.empty() && current_chunk_tokens + page_tokens > 512) {
                // Write current chunk
                if (!first_chunk) outfile << ",\n";
                first_chunk = false;
                
                rapidjson::Document chunk_doc;
                chunk_doc.SetObject();
                auto& alloc = chunk_doc.GetAllocator();
                
                // Add text
                rapidjson::Value text_val;
                text_val.SetString(current_chunk.c_str(), current_chunk.length(), alloc);
                chunk_doc.AddMember("text", text_val, alloc);
                
                // Add meta
                rapidjson::Value meta;
                meta.SetObject();
                
                rapidjson::Value schema_name;
                schema_name.SetString("docling_core.transforms.chunker.DocMeta", alloc);
                meta.AddMember("schema_name", schema_name, alloc);
                
                rapidjson::Value version;
                version.SetString("1.0.0", alloc);
                meta.AddMember("version", version, alloc);
                
                // Add origin
                rapidjson::Value origin;
                origin.SetObject();
                origin.AddMember("mimetype", "application/pdf", alloc);
                origin.AddMember("binary_hash", file_hash, alloc);
                
                rapidjson::Value filename;
                filename.SetString(fs::path(argv[1]).filename().string().c_str(), alloc);
                origin.AddMember("filename", filename, alloc);
                origin.AddMember("uri", rapidjson::Value().SetNull(), alloc);
                
                meta.AddMember("origin", origin, alloc);
                
                // Add doc_items (simplified)
                rapidjson::Value doc_items;
                doc_items.SetArray();
                meta.AddMember("doc_items", doc_items, alloc);
                
                // Add headings (empty for now)
                rapidjson::Value headings;
                headings.SetArray();
                meta.AddMember("headings", headings, alloc);
                
                meta.AddMember("captions", rapidjson::Value().SetNull(), alloc);
                
                chunk_doc.AddMember("meta", meta, alloc);
                
                // Write chunk
                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                chunk_doc.Accept(writer);
                outfile << buffer.GetString();
                
                // Reset for next chunk
                current_chunk.clear();
                current_pages.clear();
                chunk_count++;
            }
            
            // Add page to current chunk
            if (!current_chunk.empty()) current_chunk += "\n\n";
            current_chunk += page_text;
            current_pages.push_back(page_numbers[i]);
        }
        
        // Write final chunk if any
        if (!current_chunk.empty()) {
            if (!first_chunk) outfile << ",\n";
            
            rapidjson::Document chunk_doc;
            chunk_doc.SetObject();
            auto& alloc = chunk_doc.GetAllocator();
            
            rapidjson::Value text_val;
            text_val.SetString(current_chunk.c_str(), current_chunk.length(), alloc);
            chunk_doc.AddMember("text", text_val, alloc);
            
            // Similar meta structure as above
            rapidjson::Value meta;
            meta.SetObject();
            meta.AddMember("schema_name", "docling_core.transforms.chunker.DocMeta", alloc);
            meta.AddMember("version", "1.0.0", alloc);
            
            rapidjson::Value origin;
            origin.SetObject();
            origin.AddMember("mimetype", "application/pdf", alloc);
            origin.AddMember("binary_hash", file_hash, alloc);
            
            rapidjson::Value filename;
            filename.SetString(fs::path(argv[1]).filename().string().c_str(), alloc);
            origin.AddMember("filename", filename, alloc);
            origin.AddMember("uri", rapidjson::Value().SetNull(), alloc);
            
            meta.AddMember("origin", origin, alloc);
            meta.AddMember("doc_items", rapidjson::Value().SetArray(), alloc);
            meta.AddMember("headings", rapidjson::Value().SetArray(), alloc);
            meta.AddMember("captions", rapidjson::Value().SetNull(), alloc);
            
            chunk_doc.AddMember("meta", meta, alloc);
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            chunk_doc.Accept(writer);
            outfile << buffer.GetString();
            
            chunk_count++;
        }
        
        // Close JSON array
        outfile << "\n]\n";
        outfile.close();
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "\nResults:\n";
        std::cout << "Processed " << page_texts.size() << " pages into " << chunk_count << " chunks\n";
        std::cout << "Total time: " << duration.count() << "ms\n";
        
        double pages_per_second = (page_texts.size() * 1000.0) / duration.count();
        std::cout << "Performance: " << pages_per_second << " pages/second\n";
        std::cout << "Output saved to: ./out/" << pdf_name << "_chunks.json\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}