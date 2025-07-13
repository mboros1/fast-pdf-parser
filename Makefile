# Fast PDF Parser Makefile

CXX = clang++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -pthread \
           -I/opt/homebrew/include \
           -I/opt/homebrew/Cellar/mupdf-tools/1.26.3/include \
           -Iinclude

LDFLAGS = -L/opt/homebrew/lib \
          -L/opt/homebrew/Cellar/mupdf-tools/1.26.3/lib \
          -lmupdf -lmupdf-third -lz -pthread

# Source files
SRCS = src/fast_pdf_parser.cpp \
       src/thread_pool.cpp \
       src/text_extractor.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Executables
TARGETS = chunk-pdf-cli perf-test token-test benchmark-passes tokenizer-example

# Library
LIBRARY = libhierarchicalchunker.a

# Default target
all: $(TARGETS) $(LIBRARY)

# Build library object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Main CLI program
chunk-pdf-cli: $(OBJS) src/chunk_pdf_cli.o src/hierarchical_chunker.o src/cl100k_base_data.o
	$(CXX) -o $@ $^ $(LDFLAGS)

# Test programs
perf-test: $(OBJS) tests/perf_test.o
	$(CXX) -o $@ $^ $(LDFLAGS)

token-test: tests/token_test.o
	$(CXX) -o $@ $^ $(LDFLAGS)

benchmark-passes: benchmarks/benchmark_passes.o
	$(CXX) -o $@ $^ $(LDFLAGS)

tokenizer-example: examples/tokenizer_example.o
	$(CXX) -o $@ $^ $(LDFLAGS)

# Library target
$(LIBRARY): $(OBJS) src/hierarchical_chunker.o src/cl100k_base_data.o
	ar rcs $@ $^

# Clean
clean:
	rm -f $(OBJS) src/*.o benchmarks/*.o examples/*.o tests/*.o $(TARGETS) $(LIBRARY)
	rm -rf out/
	rm -f *.cmake *.sh
	rm -f cl100k_base.tiktoken

# Run test
test: chunk-pdf-cli
	./chunk-pdf-cli n3797.pdf 512 50 100

.PHONY: all clean test