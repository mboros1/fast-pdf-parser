#include <fast_pdf_parser/fast_pdf_parser.h>
#include <iostream>
#include <chrono>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input.pdf>\n";
        return 1;
    }

    try {
        fast_pdf_parser::ParseOptions options;
        options.thread_count = std::max(1u, std::thread::hardware_concurrency() - 1);
        options.batch_size = 5;
        
        fast_pdf_parser::FastPdfParser parser(options);
        
        std::cout << "Streaming parse of: " << argv[1] << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        
        size_t page_count = 0;
        parser.parse_streaming(argv[1], [&page_count](fast_pdf_parser::PageResult result) -> bool {
            if (result.success) {
                page_count++;
                if (page_count % 50 == 0) {
                    std::cout << "Processed " << page_count << " pages" << std::endl;
                }
                
                // Stop after 10 pages for testing
                if (page_count >= 10) {
                    std::cout << "Stopping after 10 pages for testing...\n";
                    return false; // Stop processing
                }
            } else {
                std::cerr << "Error on page " << result.page_number << ": " 
                         << result.error << std::endl;
            }
            return true; // Continue processing
        });
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "✓ Processed " << page_count << " pages in " << duration.count() << "ms\n";
        
        auto stats = parser.get_stats();
        std::cout << "Pages per second: " << stats["pages_per_second"] << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}