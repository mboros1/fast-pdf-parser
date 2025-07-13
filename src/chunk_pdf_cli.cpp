#include <fast_pdf_parser/hierarchical_chunker.h>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <thread>
#include <numeric>
#include <algorithm>
#include <map>

namespace fs = std::filesystem;
using namespace fast_pdf_parser;

void analyze_chunk_distribution(const std::vector<ChunkResult>& chunks) {
    if (chunks.empty()) {
        std::cout << "\nNo chunks created\n";
        return;
    }
    
    std::vector<int> token_counts;
    for (const auto& chunk : chunks) {
        token_counts.push_back(chunk.token_count);
    }
    
    std::sort(token_counts.begin(), token_counts.end());
    
    int min_tokens = token_counts.front();
    int max_tokens = token_counts.back();
    double avg_tokens = std::accumulate(token_counts.begin(), token_counts.end(), 0.0) / token_counts.size();
    
    std::cout << "\n=== Final Chunk Distribution Analysis ===\n";
    std::cout << "Total chunks: " << chunks.size() << "\n";
    std::cout << "Min tokens: " << min_tokens << "\n";
    std::cout << "Max tokens: " << max_tokens << "\n";
    std::cout << "Average tokens: " << static_cast<int>(avg_tokens) << "\n";
    
    // Calculate quintiles
    std::cout << "\nQuintiles:\n";
    for (int p = 20; p <= 80; p += 20) {
        size_t idx = (token_counts.size() - 1) * p / 100;
        std::cout << "  " << p << "th percentile: " << token_counts[idx] << " tokens\n";
    }
    
    // Token range distribution
    std::map<std::string, int> distribution;
    for (int tokens : token_counts) {
        if (tokens <= 50) distribution["1-50"]++;
        else if (tokens <= 100) distribution["51-100"]++;
        else if (tokens <= 200) distribution["101-200"]++;
        else if (tokens <= 300) distribution["201-300"]++;
        else if (tokens <= 400) distribution["301-400"]++;
        else if (tokens <= 500) distribution["401-500"]++;
        else if (tokens <= 512) distribution["501-512"]++;
        else distribution["513+"]++;
    }
    
    std::cout << "\nToken Range Distribution:\n";
    for (const auto& [range, count] : distribution) {
        double percentage = (count * 100.0) / chunks.size();
        std::cout << "  " << range << " tokens: " << count << " chunks (" 
                  << std::fixed << std::setprecision(1) << percentage << "%)\n";
    }
    
    // Check for problematic small chunks
    int small_chunks = 0;
    for (int tokens : token_counts) {
        if (tokens < 150) {
            small_chunks++;
        }
    }
    
    if (small_chunks > 0) {
        std::cout << "\nWARNING: " << small_chunks << " chunks are below the minimum threshold of 150 tokens\n";
    } else {
        std::cout << "\nSUCCESS: All chunks meet the minimum threshold of 150 tokens\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 5) {
        std::cerr << "Usage: " << argv[0] << " <input.pdf> [max_tokens=512] [overlap_tokens=0] [pages_limit=0]\n";
        return 1;
    }

    int max_tokens = 512;
    int overlap_tokens = 0;
    int pages_limit = 0;  // 0 means no limit
    
    if (argc >= 3) max_tokens = std::stoi(argv[2]);
    if (argc >= 4) overlap_tokens = std::stoi(argv[3]);
    if (argc >= 5) pages_limit = std::stoi(argv[4]);

    try {
        ChunkOptions options;
        options.max_tokens = max_tokens;
        options.overlap_tokens = overlap_tokens;
        options.min_tokens = 150;
        options.thread_count = 0; // Use hardware concurrency
        
        HierarchicalChunker chunker(options);
        
        std::cout << "Processing: " << argv[1] << " with " 
                  << (options.thread_count > 0 ? options.thread_count : std::thread::hardware_concurrency()) 
                  << " threads\n";
        std::cout << "Hierarchical chunking: max_tokens=" << max_tokens 
                  << ", overlap=" << overlap_tokens 
                  << ", min_tokens=" << options.min_tokens << "\n";
        if (pages_limit > 0) {
            std::cout << "Page limit: " << pages_limit << "\n";
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Process the PDF
        auto result = chunker.chunk_file(argv[1], pages_limit);
        
        if (!result.error.empty()) {
            std::cerr << "Error: " << result.error << std::endl;
            return 1;
        }
        
        std::cout << "Extracted " << result.total_pages << " pages, creating hierarchical chunks...\n";
        
        // Analyze distribution
        analyze_chunk_distribution(result.chunks);
        
        // Save to JSON
        fs::create_directories("./out");
        std::string pdf_name = fs::path(argv[1]).stem().string();
        std::string output_path = "./out/" + pdf_name + "_hierarchical_chunks.json";
        
        if (chunker.process_pdf_to_json(argv[1], output_path, pages_limit)) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            std::cout << "\nResults:\n";
            std::cout << "Created " << result.total_chunks << " chunks from " << result.total_pages << " pages\n";
            std::cout << "Total time: " << duration.count() << "ms\n";
            std::cout << "Performance: " << (result.total_pages * 1000.0) / duration.count() << " pages/second\n";
            std::cout << "Output: " << output_path << "\n";
        } else {
            std::cerr << "Failed to save JSON output\n";
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}