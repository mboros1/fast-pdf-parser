#include <fast_pdf_parser/fast_pdf_parser.h>
#include <fast_pdf_parser/json_serializer.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>

namespace fs = std::filesystem;

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <input.pdf|input_directory> [output_directory]\n"
              << "\nOptions:\n"
              << "  input.pdf         Process a single PDF file\n"
              << "  input_directory   Process all PDFs in directory recursively\n"
              << "  output_directory  Directory to save JSON output (default: ./out)\n"
              << "\nExamples:\n"
              << "  " << program_name << " document.pdf\n"
              << "  " << program_name << " /path/to/pdfs /path/to/output\n";
}

void process_single_file(const std::string& pdf_path, const std::string& output_dir) {
    fast_pdf_parser::FastPdfParser parser;
    
    std::cout << "Processing: " << pdf_path << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        auto result = parser.parse(pdf_path);
        
        // Chunk the document
        auto chunks = fast_pdf_parser::JsonSerializer::chunk_document(result);
        
        // Create output directory
        fs::create_directories(output_dir);
        
        // Generate output filename
        auto pdf_filename = fs::path(pdf_path).stem().string();
        auto output_path = fs::path(output_dir) / (pdf_filename + "_chunks.json");
        
        // Write chunks
        std::ofstream out(output_path);
        out << fast_pdf_parser::JsonSerializer::serialize_chunks(chunks);
        out.close();
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "✓ Saved " << chunks.size() << " chunks to " << output_path << std::endl;
        std::cout << "  Processing time: " << duration.count() << "ms" << std::endl;
        
        // Print statistics
        auto stats = parser.get_stats();
        std::cout << "  Pages processed: " << stats["pages_processed"] << std::endl;
        std::cout << "  Pages per second: " << std::fixed << std::setprecision(1) 
                  << stats["pages_per_second"] << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Error processing " << pdf_path << ": " << e.what() << std::endl;
    }
}

void process_directory(const std::string& input_dir, const std::string& output_dir) {
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
    
    std::cout << "Found " << pdf_files.size() << " PDF files to process\n" << std::endl;
    
    // Create output directory
    fs::create_directories(output_dir);
    
    // Process files
    fast_pdf_parser::FastPdfParser parser;
    auto start_total = std::chrono::high_resolution_clock::now();
    
    auto results = parser.parse_batch(pdf_files, [](size_t current, size_t total) {
        std::cout << "\rProgress: " << current << "/" << total 
                  << " (" << (100 * current / total) << "%)" << std::flush;
    });
    std::cout << std::endl;
    
    // Save results
    size_t success_count = 0;
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        const auto& pdf_path = pdf_files[i];
        
        if (result.contains("error")) {
            std::cerr << "✗ Error processing " << pdf_path << ": " 
                     << result["error"] << std::endl;
            continue;
        }
        
        // Chunk and save
        auto chunks = fast_pdf_parser::JsonSerializer::chunk_document(result);
        auto pdf_filename = fs::path(pdf_path).stem().string();
        auto output_path = fs::path(output_dir) / (pdf_filename + "_chunks.json");
        
        std::ofstream out(output_path);
        out << fast_pdf_parser::JsonSerializer::serialize_chunks(chunks);
        out.close();
        
        success_count++;
    }
    
    auto end_total = std::chrono::high_resolution_clock::now();
    auto duration_total = std::chrono::duration_cast<std::chrono::seconds>(end_total - start_total);
    
    // Print summary
    std::cout << "\n=== Processing Complete ===\n";
    std::cout << "Successfully processed: " << success_count << "/" << pdf_files.size() << " files\n";
    std::cout << "Total time: " << duration_total.count() << " seconds\n";
    
    auto stats = parser.get_stats();
    std::cout << "Total pages: " << stats["pages_processed"] << std::endl;
    std::cout << "Average pages/second: " << std::fixed << std::setprecision(1) 
              << stats["pages_per_second"] << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string input_path = argv[1];
    std::string output_dir = argc == 3 ? argv[2] : "./out";
    
    if (!fs::exists(input_path)) {
        std::cerr << "Error: Input path does not exist: " << input_path << std::endl;
        return 1;
    }
    
    if (fs::is_regular_file(input_path)) {
        // Process single file
        process_single_file(input_path, output_dir);
    } else if (fs::is_directory(input_path)) {
        // Process directory
        process_directory(input_path, output_dir);
    } else {
        std::cerr << "Error: Input must be a PDF file or directory" << std::endl;
        return 1;
    }
    
    return 0;
}