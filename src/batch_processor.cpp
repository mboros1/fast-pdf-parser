#include "fast_pdf_parser/fast_pdf_parser.h"
#include "fast_pdf_parser/json_serializer.h"
#include <filesystem>
#include <iostream>
#include <fstream>

namespace fast_pdf_parser {

void process_directory(const std::string& input_dir, const std::string& output_dir,
                      const ParseOptions& options = ParseOptions{}) {
    namespace fs = std::filesystem;
    
    // Create output directory if it doesn't exist
    fs::create_directories(output_dir);
    
    // Collect all PDF files
    std::vector<std::string> pdf_files;
    for (const auto& entry : fs::recursive_directory_iterator(input_dir)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".pdf") {
                pdf_files.push_back(entry.path().string());
            }
        }
    }
    
    if (pdf_files.empty()) {
        std::cout << "No PDF files found in " << input_dir << std::endl;
        return;
    }
    
    std::cout << "Found " << pdf_files.size() << " PDF files to process" << std::endl;
    
    // Create parser
    FastPdfParser parser(options);
    
    // Process files with progress reporting
    auto progress = [](size_t current, size_t total) {
        std::cout << "Progress: " << current << "/" << total 
                  << " (" << (100 * current / total) << "%)" << std::endl;
    };
    
    auto results = parser.parse_batch(pdf_files, progress);
    
    // Process and save results
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        const auto& pdf_path = pdf_files[i];
        
        if (result.contains("error")) {
            std::cerr << "Error processing " << pdf_path << ": " 
                     << result["error"] << std::endl;
            continue;
        }
        
        // Chunk the document
        auto chunks = JsonSerializer::chunk_document(result, 512, true);
        
        // Generate output filename
        auto pdf_filename = fs::path(pdf_path).stem().string();
        auto output_path = fs::path(output_dir) / (pdf_filename + "_chunks.json");
        
        // Write chunks to file
        std::ofstream out(output_path);
        out << JsonSerializer::serialize_chunks(chunks);
        out.close();
        
        std::cout << "Saved " << chunks.size() << " chunks to " << output_path << std::endl;
    }
    
    // Print statistics
    auto stats = parser.get_stats();
    std::cout << "\nProcessing Statistics:" << std::endl;
    std::cout << "Documents processed: " << stats["documents_processed"] << std::endl;
    std::cout << "Pages processed: " << stats["pages_processed"] << std::endl;
    std::cout << "Average processing time: " << stats["average_processing_time_ms"] << " ms" << std::endl;
    std::cout << "Pages per second: " << stats["pages_per_second"] << std::endl;
}

} // namespace fast_pdf_parser