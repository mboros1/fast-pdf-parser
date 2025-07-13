# Fast PDF Parser

A high-performance PDF text extraction and chunking library using MuPDF with Node.js bindings.

## Features

- Multi-threaded PDF processing using MuPDF
- 7-pass hierarchical chunking algorithm
- Eliminates bimodal chunk size distribution
- Semantic boundary preservation
- ~35 pages/second processing with chunking
- Docling-compatible JSON output format
- Memory-efficient streaming processing
- Thread pool for parallel page extraction

## Prerequisites

### Install MuPDF

MuPDF is required for PDF processing and must be installed separately:

**macOS (Homebrew):**
```bash
brew install mupdf-tools
```

**Ubuntu/Debian:**
```bash
sudo apt-get install libmupdf-dev
```

**From Source:**
```bash
git clone --recursive https://git.ghostscript.com/mupdf.git
cd mupdf
make HAVE_X11=no HAVE_GLUT=no prefix=/usr/local install
```

## Building

```bash
make hierarchical-chunker
```

## Usage

```bash
./hierarchical-chunker input.pdf [max_tokens] [overlap_tokens] [page_limit]

# Example: Process first 100 pages with 512 max tokens and 50 token overlap
./hierarchical-chunker document.pdf 512 50 100
```

### Parameters
- `max_tokens`: Maximum tokens per chunk (default: 512)
- `overlap_tokens`: Overlap between chunks (default: 50) 
- `page_limit`: Limit pages to process (default: 0 = all pages)

### Output
Creates JSON file in `./out/` directory with chunked content and metadata.

## Performance

Achieved performance:
- ~35 pages/second with full hierarchical chunking
- Consistent chunk sizes (400-550 tokens)
- Eliminates bimodal distribution
- Linear scaling with thread count
- Minimal memory overhead

## Node.js Bindings

### Installation

```bash
npm install
```

### Usage

```javascript
const { HierarchicalChunker, chunkPdf } = require('fast-pdf-parser');

// Create a chunker instance
const chunker = new HierarchicalChunker({
    maxTokens: 512,      // Maximum tokens per chunk
    minTokens: 150,      // Minimum tokens per chunk
    overlapTokens: 0,    // Token overlap between chunks
    threadCount: 4       // Number of worker threads
});

// Chunk a PDF file
const result = chunker.chunkFile('document.pdf');

console.log(`Created ${result.totalChunks} chunks from ${result.totalPages} pages`);
console.log(`Processing took ${result.processingTimeMs}ms`);

// Access chunks
result.chunks.forEach(chunk => {
    console.log(`Chunk: ${chunk.tokenCount} tokens, pages ${chunk.startPage}-${chunk.endPage}`);
    console.log(`Text: ${chunk.text.substring(0, 100)}...`);
});

// Or use the convenience function
const result2 = chunkPdf('document.pdf', { 
    maxTokens: 400,
    pageLimit: 10  // Process only first 10 pages
});
```

### TypeScript Support

Full TypeScript definitions are included in `lib/index.d.ts`.

## C++ Library API

```cpp
#include "fast_pdf_parser/hierarchical_chunker.h"

using namespace fast_pdf_parser;

// Configure options
ChunkOptions options;
options.max_tokens = 512;
options.min_tokens = 150;
options.thread_count = 8;

// Create chunker
HierarchicalChunker chunker(options);

// Process PDF
ChunkingResult result = chunker.chunk_file("document.pdf");

if (result.error.empty()) {
    std::cout << "Created " << result.total_chunks << " chunks\n";
    for (const auto& chunk : result.chunks) {
        std::cout << "Chunk: " << chunk.token_count << " tokens\n";
    }
}
```

## Development

See ONBOARDING.md for detailed development setup instructions.

## License

MIT
