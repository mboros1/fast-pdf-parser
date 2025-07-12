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
TARGETS = hierarchical-chunker perf-test token-test benchmark-passes tokenizer-example

# Default target
all: $(TARGETS)

# Build library object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Main implementation
hierarchical-chunker: $(OBJS) src/hierarchical_chunker.o
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

# Clean
clean:
	rm -f $(OBJS) src/*.o benchmarks/*.o examples/*.o tests/*.o $(TARGETS)
	rm -rf out/
	rm -f *.cmake *.sh
	rm -f cl100k_base.tiktoken

# Run test
test: hierarchical-chunker
	./hierarchical-chunker n3797.pdf 512 50 100

.PHONY: all clean test