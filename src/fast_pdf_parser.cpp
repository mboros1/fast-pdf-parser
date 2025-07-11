#include "fast_pdf_parser/fast_pdf_parser.h"
#include "fast_pdf_parser/thread_pool.h"
#include "fast_pdf_parser/text_extractor.h"
#include "fast_pdf_parser/json_serializer.h"
#include <filesystem>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iostream>

namespace fast_pdf_parser {

class FastPdfParser::Impl {
public:
    Impl(const ParseOptions& options) 
        : options_(options), 
          thread_pool_(options.thread_count) {
        stats_["pages_processed"] = 0;
        stats_["documents_processed"] = 0;
        stats_["total_processing_time_ms"] = 0;
    }

    nlohmann::json parse(const std::string& pdf_path) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        if (!std::filesystem::exists(pdf_path)) {
            throw std::runtime_error("PDF file not found: " + pdf_path);
        }

        // Calculate file hash
        int64_t file_hash = calculate_file_hash(pdf_path);
        
        // Extract text from all pages
        TextExtractor extractor;
        ExtractOptions extract_opts;
        extract_opts.extract_positions = options_.extract_positions;
        extract_opts.extract_fonts = options_.extract_fonts;
        extract_opts.extract_colors = options_.extract_colors;
        
        auto raw_output = extractor.extract_all_pages(pdf_path, extract_opts);
        
        // Convert to Docling format
        auto filename = std::filesystem::path(pdf_path).filename().string();
        auto docling_output = JsonSerializer::to_docling_format(raw_output, filename, file_hash);
        
        // Update statistics
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        stats_["documents_processed"] = stats_["documents_processed"].get<int>() + 1;
        stats_["pages_processed"] = stats_["pages_processed"].get<int>() + raw_output["pages"].size();
        stats_["total_processing_time_ms"] = stats_["total_processing_time_ms"].get<int>() + duration.count();
        
        return docling_output;
    }

    void parse_streaming(const std::string& pdf_path, PageCallback callback) {
        std::cout << "[FastPdfParser::parse_streaming] Starting streaming parse for " << pdf_path << std::endl;
        
        if (!std::filesystem::exists(pdf_path)) {
            throw std::runtime_error("PDF file not found: " + pdf_path);
        }
        std::cout << "[FastPdfParser::parse_streaming] File exists" << std::endl;

        TextExtractor extractor;
        ExtractOptions extract_opts;
        extract_opts.extract_positions = options_.extract_positions;
        extract_opts.extract_fonts = options_.extract_fonts;
        extract_opts.extract_colors = options_.extract_colors;
        
        std::cout << "[FastPdfParser::parse_streaming] Extract options set - positions: " << extract_opts.extract_positions 
                  << ", fonts: " << extract_opts.extract_fonts << std::endl;

        // Get page count first
        std::cout << "[FastPdfParser::parse_streaming] Getting page count" << std::endl;
        int page_count = extractor.get_page_count(pdf_path);
        std::cout << "[FastPdfParser::parse_streaming] Page count: " << page_count << std::endl;
        
        // Process pages in parallel batches
        std::vector<std::future<PageResult>> futures;
        
        for (size_t i = 0; i < page_count; i += options_.batch_size) {
            size_t batch_end = std::min(i + options_.batch_size, static_cast<size_t>(page_count));
            
            for (size_t page_idx = i; page_idx < batch_end; ++page_idx) {
                futures.push_back(
                    thread_pool_.enqueue([this, pdf_path, page_idx, extract_opts]() {
                        PageResult result;
                        result.page_number = page_idx;
                        
                        try {
                            TextExtractor page_extractor;
                            result.content = page_extractor.extract_page(pdf_path, page_idx, extract_opts);
                            result.success = true;
                        } catch (const std::exception& e) {
                            result.error = e.what();
                            result.success = false;
                        }
                        
                        return result;
                    })
                );
            }
            
            // Wait for batch completion and invoke callbacks
            for (auto& future : futures) {
                callback(future.get());
            }
            futures.clear();
        }
    }

    std::vector<nlohmann::json> parse_batch(const std::vector<std::string>& pdf_paths,
                                           ProgressCallback progress) {
        std::vector<nlohmann::json> results;
        std::mutex results_mutex;
        std::atomic<size_t> completed{0};
        
        std::vector<std::future<void>> futures;
        
        for (const auto& path : pdf_paths) {
            futures.push_back(
                thread_pool_.enqueue([this, path, &results, &results_mutex, &completed, &pdf_paths, progress]() {
                    try {
                        auto result = parse(path);
                        
                        {
                            std::lock_guard<std::mutex> lock(results_mutex);
                            results.push_back(result);
                        }
                    } catch (const std::exception& e) {
                        nlohmann::json error_result;
                        error_result["error"] = e.what();
                        error_result["file"] = path;
                        
                        std::lock_guard<std::mutex> lock(results_mutex);
                        results.push_back(error_result);
                    }
                    
                    completed++;
                    if (progress) {
                        progress(completed, pdf_paths.size());
                    }
                })
            );
        }
        
        // Wait for all documents to complete
        for (auto& future : futures) {
            future.get();
        }
        
        return results;
    }

    nlohmann::json get_stats() const {
        nlohmann::json stats = stats_;
        
        if (stats["documents_processed"] > 0) {
            double avg_time = static_cast<double>(stats["total_processing_time_ms"]) / 
                            static_cast<double>(stats["documents_processed"]);
            stats["average_processing_time_ms"] = avg_time;
            
            double pages_per_second = static_cast<double>(stats["pages_processed"]) / 
                                    (static_cast<double>(stats["total_processing_time_ms"]) / 1000.0);
            stats["pages_per_second"] = pages_per_second;
        }
        
        return stats;
    }

private:
    int64_t calculate_file_hash(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        std::stringstream buffer;
        buffer << file.rdbuf();
        
        // Simple hash for demo - in production use proper hash
        std::hash<std::string> hasher;
        return static_cast<int64_t>(hasher(buffer.str()));
    }

    ParseOptions options_;
    ThreadPool thread_pool_;
    mutable nlohmann::json stats_;
};

FastPdfParser::FastPdfParser(const ParseOptions& options) 
    : pImpl(std::make_unique<Impl>(options)) {}

FastPdfParser::~FastPdfParser() = default;

nlohmann::json FastPdfParser::parse(const std::string& pdf_path) {
    return pImpl->parse(pdf_path);
}

void FastPdfParser::parse_streaming(const std::string& pdf_path, PageCallback callback) {
    pImpl->parse_streaming(pdf_path, callback);
}

std::vector<nlohmann::json> FastPdfParser::parse_batch(const std::vector<std::string>& pdf_paths,
                                                       ProgressCallback progress) {
    return pImpl->parse_batch(pdf_paths, progress);
}

nlohmann::json FastPdfParser::get_stats() const {
    return pImpl->get_stats();
}

} // namespace fast_pdf_parser