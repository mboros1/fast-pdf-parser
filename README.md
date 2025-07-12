# Fast PDF Parser - Hierarchical Chunker

High-performance C++ PDF text extraction and hierarchical chunking library with 7-pass semantic chunking algorithm.

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

## Development

See ONBOARDING.md for detailed development setup instructions.
