#include <fast_pdf_parser/fast_pdf_parser.h>
#include <fast_pdf_parser/hierarchical_chunker.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

namespace fs = std::filesystem;

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
        
        // Collect all pages first
        std::vector<std::string> page_texts;
        std::vector<int> page_numbers;
        size_t total_pages = 0;
        
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
                
                if (++total_pages % 100 == 0) {
                    std::cout << "Extracted " << total_pages << " pages...\n";
                }
            }
            return true;
        });
        
        std::cout << "Extracted " << page_texts.size() << " pages, now chunking hierarchically...\n";
        
        // Use hierarchical chunker
        fast_pdf_parser::HierarchicalChunker chunker(max_tokens, overlap_tokens, true);
        auto chunks = chunker.chunk_document(page_texts, page_numbers);
        
        // Calculate file hash
        std::hash<std::string> hasher;
        int64_t file_hash = static_cast<int64_t>(hasher(argv[1]));
        
        // Write output
        std::string pdf_name = fs::path(argv[1]).stem().string();
        std::ofstream outfile("./out/" + pdf_name + "_hierarchical_chunks.json");
        
        // Start JSON array
        outfile << "[\n";
        
        for (size_t i = 0; i < chunks.size(); ++i) {
            const auto& chunk = chunks[i];
            
            if (i > 0) outfile << ",\n";
            
            rapidjson::Document chunk_doc;
            chunk_doc.SetObject();
            auto& alloc = chunk_doc.GetAllocator();
            
            // Add text
            rapidjson::Value text_val;
            text_val.SetString(chunk.text.c_str(), chunk.text.length(), alloc);
            chunk_doc.AddMember("text", text_val, alloc);
            
            // Add metadata
            rapidjson::Value meta;
            meta.SetObject();
            
            rapidjson::Value schema_name;
            schema_name.SetString("docling_core.transforms.chunker.DocMeta", alloc);
            meta.AddMember("schema_name", schema_name, alloc);
            
            rapidjson::Value version;
            version.SetString("1.0.0", alloc);
            meta.AddMember("version", version, alloc);
            
            // Add page numbers
            rapidjson::Value pages(rapidjson::kArrayType);
            for (int page : chunk.page_numbers) {
                pages.PushBack(page, alloc);
            }
            meta.AddMember("page_numbers", pages, alloc);
            
            // Add chunk metadata
            meta.AddMember("chunk_index", static_cast<int>(i), alloc);
            meta.AddMember("total_chunks", static_cast<int>(chunks.size()), alloc);
            meta.AddMember("token_count", static_cast<int>(chunk.token_count), alloc);
            meta.AddMember("starts_with_heading", chunk.starts_with_heading, alloc);
            meta.AddMember("ends_cleanly", chunk.ends_cleanly, alloc);
            
            // Add context headings if any
            if (!chunk.context_headings.empty()) {
                rapidjson::Value context(rapidjson::kArrayType);
                for (const auto& heading : chunk.context_headings) {
                    rapidjson::Value h;
                    h.SetString(heading.c_str(), heading.length(), alloc);
                    context.PushBack(h, alloc);
                }
                meta.AddMember("context_headings", context, alloc);
            }
            
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
            
            // Empty arrays for compatibility
            meta.AddMember("doc_items", rapidjson::Value(rapidjson::kArrayType), alloc);
            meta.AddMember("headings", rapidjson::Value(rapidjson::kArrayType), alloc);
            meta.AddMember("captions", rapidjson::Value().SetNull(), alloc);
            
            chunk_doc.AddMember("meta", meta, alloc);
            
            // Write chunk
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            chunk_doc.Accept(writer);
            outfile << buffer.GetString();
        }
        
        // Close JSON array
        outfile << "\n]\n";
        outfile.close();
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "\nResults:\n";
        std::cout << "Processed " << page_texts.size() << " pages into " << chunks.size() << " chunks\n";
        std::cout << "Total time: " << duration.count() << "ms\n";
        
        double pages_per_second = (page_texts.size() * 1000.0) / duration.count();
        std::cout << "Performance: " << pages_per_second << " pages/second\n";
        std::cout << "Output saved to: ./out/" << pdf_name << "_hierarchical_chunks.json\n";
        
        // Print chunk statistics
        size_t min_tokens = chunks[0].token_count;
        size_t max_tokens = chunks[0].token_count;
        size_t total_tokens = 0;
        
        for (const auto& chunk : chunks) {
            min_tokens = std::min(min_tokens, chunk.token_count);
            max_tokens = std::max(max_tokens, chunk.token_count);
            total_tokens += chunk.token_count;
        }
        
        std::cout << "\nChunk statistics:\n";
        std::cout << "  Min tokens: " << min_tokens << "\n";
        std::cout << "  Max tokens: " << max_tokens << "\n";
        std::cout << "  Avg tokens: " << total_tokens / chunks.size() << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}