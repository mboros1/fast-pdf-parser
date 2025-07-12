#include <fast_pdf_parser/fast_pdf_parser.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input.pdf>\n";
        return 1;
    }

    try {
        fast_pdf_parser::ParseOptions options;
        options.thread_count = std::max(1u, std::thread::hardware_concurrency() - 1);
        options.batch_size = 10;
        options.extract_positions = false;  // Faster without positions
        options.extract_fonts = false;      // Faster without fonts
        
        fast_pdf_parser::FastPdfParser parser(options);
        
        std::cout << "Processing: " << argv[1] << " with " << options.thread_count << " threads\n";
        auto start = std::chrono::high_resolution_clock::now();
        
        // Create output directory
        fs::create_directories("./out");
        
        // Open output file
        std::string pdf_name = fs::path(argv[1]).stem().string();
        std::ofstream outfile("./out/" + pdf_name + "_pages.json");
        
        // Start JSON array
        outfile << "[\n";
        
        size_t page_count = 0;
        bool first = true;
        
        parser.parse_streaming(argv[1], [&](fast_pdf_parser::PageResult result) -> bool {
            if (result.success) {
                if (!first) {
                    outfile << ",\n";
                }
                first = false;
                
                // Write page JSON using RapidJSON for speed
                rapidjson::Document page_doc;
                page_doc.SetObject();
                auto& alloc = page_doc.GetAllocator();
                
                page_doc.AddMember("page_number", result.page_number, alloc);
                
                // Extract text from the nlohmann json result
                std::string page_text;
                if (result.content.contains("blocks")) {
                    for (const auto& block : result.content["blocks"]) {
                        for (const auto& line : block["lines"]) {
                            if (!page_text.empty()) page_text += "\n";
                            page_text += line["text"].get<std::string>();
                        }
                    }
                }
                
                rapidjson::Value text_val;
                text_val.SetString(page_text.c_str(), page_text.length(), alloc);
                page_doc.AddMember("text", text_val, alloc);
                
                // Fast write to file
                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                page_doc.Accept(writer);
                
                outfile << buffer.GetString();
                
                page_count++;
                if (page_count % 50 == 0) {
                    std::cout << "Processed " << page_count << " pages\n";
                }
            }
            return true; // Continue processing
        });
        
        // Close JSON array
        outfile << "\n]\n";
        outfile.close();
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "\nResults:\n";
        std::cout << "Processed " << page_count << " pages in " << duration.count() << "ms\n";
        
        double pages_per_second = (page_count * 1000.0) / duration.count();
        std::cout << "Performance: " << pages_per_second << " pages/second\n";
        std::cout << "Output saved to: ./out/" << pdf_name << "_pages.json\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}