# Fast PDF Parser

High-performance C++ PDF text extraction library targeting 10-100x speedup over Docling.

## Features

- Multi-threaded PDF processing using MuPDF
- 50-500 pages/second performance for text-heavy PDFs
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

### Install Conan

```bash
pip install conan
```

## Building

1. Install dependencies with Conan:
```bash
conan install . --output-folder=build --build=missing
```

2. Configure and build:
```bash
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## Usage

### Command Line Tool

```bash
./build/fast-pdf-parser input.pdf
```

### As a Library

```cpp
#include <fast_pdf_parser/fast_pdf_parser.h>

fast_pdf_parser::FastPdfParser parser;
auto result = parser.parse("document.pdf");
```

### Batch Processing

```cpp
std::vector<std::string> pdfs = {"doc1.pdf", "doc2.pdf", "doc3.pdf"};
auto results = parser.parse_batch(pdfs, [](size_t current, size_t total) {
    std::cout << "Progress: " << current << "/" << total << std::endl;
});
```

## Performance

Target performance metrics:
- 50-500 pages/second for text-heavy PDFs
- <50MB memory usage per page
- Linear scaling with thread count
- No degradation on large files

## Development

See ONBOARDING.md for detailed development setup instructions.
