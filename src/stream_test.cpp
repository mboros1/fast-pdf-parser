#include <fast_pdf_parser/fast_pdf_parser.h>
#include <iostream>
#include <chrono>

int main(int argc, char* argv[]) {
    std::cout << "[main] Program started" << std::endl;
    std::cout << "[main] argc = " << argc << std::endl;
    
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input.pdf>\n";
        return 1;
    }
    
    std::cout << "[main] Input file: " << argv[1] << std::endl;

    try {
        std::cout << "[main] Creating ParseOptions" << std::endl;
        fast_pdf_parser::ParseOptions options;
        options.thread_count = 4;
        options.batch_size = 5;
        std::cout << "[main] Options: thread_count=" << options.thread_count 
                  << ", batch_size=" << options.batch_size << std::endl;
        
        std::cout << "[main] Creating FastPdfParser" << std::endl;
        fast_pdf_parser::FastPdfParser parser(options);
        std::cout << "[main] FastPdfParser created successfully" << std::endl;
        
        std::cout << "Streaming parse of: " << argv[1] << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        
        size_t page_count = 0;
        std::cout << "[main] About to call parse_streaming" << std::endl;
        parser.parse_streaming(argv[1], [&page_count](fast_pdf_parser::PageResult result) {
            std::cout << "[main/callback] Callback invoked for page " << result.page_number << std::endl;
            if (result.success) {
                page_count++;
                std::cout << "Processed page " << result.page_number << std::endl;
                
                // Stop after 10 pages for testing
                if (page_count >= 10) {
                    std::cout << "Stopping after 10 pages for testing...\n";
                    exit(0);
                }
            } else {
                std::cerr << "Error on page " << result.page_number << ": " 
                         << result.error << std::endl;
            }
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