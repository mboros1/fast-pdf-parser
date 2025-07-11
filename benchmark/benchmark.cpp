#include <benchmark/benchmark.h>
#include <fast_pdf_parser/fast_pdf_parser.h>
#include <fast_pdf_parser/json_serializer.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// Create a test PDF path - you'll need to provide actual test PDFs
const std::string TEST_PDF_SMALL = "test_data/small.pdf";  // ~10 pages
const std::string TEST_PDF_MEDIUM = "test_data/medium.pdf"; // ~100 pages
const std::string TEST_PDF_LARGE = "test_data/large.pdf";   // ~1000 pages

static void BM_SinglePageExtraction(benchmark::State& state) {
    if (!fs::exists(TEST_PDF_SMALL)) {
        state.SkipWithError("Test PDF not found");
        return;
    }
    
    fast_pdf_parser::ParseOptions options;
    options.thread_count = state.range(0);
    fast_pdf_parser::FastPdfParser parser(options);
    
    for (auto _ : state) {
        auto result = parser.parse(TEST_PDF_SMALL);
        benchmark::DoNotOptimize(result);
    }
    
    auto stats = parser.get_stats();
    state.counters["pages_per_second"] = stats["pages_per_second"];
}
BENCHMARK(BM_SinglePageExtraction)->Range(1, 8);

static void BM_BatchProcessing(benchmark::State& state) {
    std::vector<std::string> test_files;
    
    // Add same file multiple times to simulate batch
    for (int i = 0; i < state.range(1); ++i) {
        if (fs::exists(TEST_PDF_SMALL)) {
            test_files.push_back(TEST_PDF_SMALL);
        }
    }
    
    if (test_files.empty()) {
        state.SkipWithError("Test PDFs not found");
        return;
    }
    
    fast_pdf_parser::ParseOptions options;
    options.thread_count = state.range(0);
    fast_pdf_parser::FastPdfParser parser(options);
    
    for (auto _ : state) {
        auto results = parser.parse_batch(test_files);
        benchmark::DoNotOptimize(results);
    }
    
    auto stats = parser.get_stats();
    state.counters["pages_per_second"] = stats["pages_per_second"];
    state.counters["documents"] = test_files.size();
}
BENCHMARK(BM_BatchProcessing)->Ranges({{1, 8}, {1, 10}});

static void BM_StreamingParse(benchmark::State& state) {
    if (!fs::exists(TEST_PDF_MEDIUM)) {
        state.SkipWithError("Test PDF not found");
        return;
    }
    
    fast_pdf_parser::ParseOptions options;
    options.thread_count = state.range(0);
    options.batch_size = state.range(1);
    fast_pdf_parser::FastPdfParser parser(options);
    
    for (auto _ : state) {
        size_t page_count = 0;
        parser.parse_streaming(TEST_PDF_MEDIUM, [&page_count](fast_pdf_parser::PageResult result) {
            if (result.success) {
                page_count++;
            }
        });
        benchmark::DoNotOptimize(page_count);
    }
    
    auto stats = parser.get_stats();
    state.counters["pages_per_second"] = stats["pages_per_second"];
}
BENCHMARK(BM_StreamingParse)->Ranges({{1, 8}, {5, 20}});

static void BM_JsonChunking(benchmark::State& state) {
    if (!fs::exists(TEST_PDF_SMALL)) {
        state.SkipWithError("Test PDF not found");
        return;
    }
    
    fast_pdf_parser::FastPdfParser parser;
    auto document = parser.parse(TEST_PDF_SMALL);
    
    size_t max_tokens = state.range(0);
    
    for (auto _ : state) {
        auto chunks = fast_pdf_parser::JsonSerializer::chunk_document(document, max_tokens);
        benchmark::DoNotOptimize(chunks);
    }
}
BENCHMARK(BM_JsonChunking)->Range(128, 1024);

static void BM_LargeDocumentScaling(benchmark::State& state) {
    std::string test_pdf;
    if (state.range(1) < 100 && fs::exists(TEST_PDF_SMALL)) {
        test_pdf = TEST_PDF_SMALL;
    } else if (state.range(1) < 500 && fs::exists(TEST_PDF_MEDIUM)) {
        test_pdf = TEST_PDF_MEDIUM;
    } else if (fs::exists(TEST_PDF_LARGE)) {
        test_pdf = TEST_PDF_LARGE;
    } else {
        state.SkipWithError("No suitable test PDF found");
        return;
    }
    
    fast_pdf_parser::ParseOptions options;
    options.thread_count = state.range(0);
    fast_pdf_parser::FastPdfParser parser(options);
    
    for (auto _ : state) {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = parser.parse(test_pdf);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        state.SetIterationTime(duration.count() / 1000.0);
        benchmark::DoNotOptimize(result);
    }
    
    auto stats = parser.get_stats();
    state.counters["pages_per_second"] = stats["pages_per_second"];
    state.counters["total_pages"] = stats["pages_processed"];
}
BENCHMARK(BM_LargeDocumentScaling)->Ranges({{1, 8}, {10, 1000}})->UseManualTime();

BENCHMARK_MAIN();