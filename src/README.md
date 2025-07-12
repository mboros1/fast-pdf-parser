# Source Code Organization

This project contains two separate PDF chunking implementations:

## 1. Hierarchical Chunker (hierarchical_chunker.cpp)
- **Our main implementation** using 7-pass hierarchical chunking
- Uses **rapidjson** for JSON output
- Eliminates bimodal distribution problem
- Produces chunks primarily in 400-550 token range
- Compilation: `make hierarchical-chunker`
- Usage: `./hierarchical-chunker input.pdf [max_tokens] [overlap] [page_limit]`

### Dependencies:
- fast_pdf_parser.cpp (PDF parsing)
- thread_pool.cpp (multi-threading)
- text_extractor.cpp (MuPDF text extraction)

## 2. Original Fast PDF Parser (main.cpp)
- Original implementation using simple chunking
- Uses **nlohmann json** library
- Different chunking approach via JsonSerializer
- Compilation: `make fast-pdf-parser`
- Usage: `./fast-pdf-parser input.pdf [output_dir]`

### Additional Dependencies:
- json_serializer.cpp (nlohmann json chunking)
- batch_processor.cpp (batch processing support)

## Test Files
- **perf_test.cpp** - Performance benchmarking with different thread counts
- **token_test.cpp** - Tiktoken tokenizer testing

## Notes
- The two implementations use different JSON libraries (rapidjson vs nlohmann)
- hierarchical_chunker is the recommended implementation for production use
- Both share the core PDF parsing infrastructure