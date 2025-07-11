#include <fast_pdf_parser/fast_pdf_parser.h>
#include <iostream>
#include <fstream>
#include <chrono>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input.pdf>\n";
        return 1;
    }

    try {
        fast_pdf_parser::FastPdfParser parser;
        
        std::cout << "Parsing: " << argv[1] << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        
        auto result = parser.parse(argv[1]);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "✓ Parsed successfully in " << duration.count() << "ms\n";
        
        // Print basic info
        if (result.contains("content") && result["content"].contains("doc_items")) {
            std::cout << "Document items: " << result["content"]["doc_items"].size() << std::endl;
        }
        
        // Get stats
        auto stats = parser.get_stats();
        std::cout << "Pages processed: " << stats["pages_processed"] << std::endl;
        std::cout << "Pages per second: " << stats["pages_per_second"] << std::endl;
        
        // Save raw JSON output
        std::ofstream out("output.json");
        out << result.dump(2);
        out.close();
        std::cout << "Raw output saved to output.json\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}