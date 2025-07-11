#include <fast_pdf_parser/fast_pdf_parser.h>
#include <iostream>
#include <chrono>
#include <atomic>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input.pdf>\n";
        return 1;
    }

    try {
        fast_pdf_parser::ParseOptions options;
        options.thread_count = 4;
        options.batch_size = 10;
        options.extract_positions = false;  // Faster without positions
        options.extract_fonts = false;      // Faster without fonts
        
        fast_pdf_parser::FastPdfParser parser(options);
        
        std::cout << "Testing with 4 threads, processing first 100 pages..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        
        std::atomic<size_t> page_count{0};
        std::atomic<bool> should_stop{false};
        
        parser.parse_streaming(argv[1], [&page_count, &should_stop](fast_pdf_parser::PageResult result) {
            if (should_stop.load()) return;
            
            if (result.success) {
                page_count++;
                if (page_count % 10 == 0) {
                    std::cout << "Processed " << page_count << " pages..." << std::endl;
                }
                
                if (page_count >= 100) {
                    should_stop = true;
                    std::cout << "Reached 100 pages, stopping..." << std::endl;
                }
            }
        });
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "\nResults:" << std::endl;
        std::cout << "Processed " << page_count << " pages in " << duration.count() << "ms" << std::endl;
        
        double pages_per_second = (page_count * 1000.0) / duration.count();
        std::cout << "Performance: " << pages_per_second << " pages/second" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}