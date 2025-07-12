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
       src/text_extractor.cpp \
       src/json_serializer.cpp \
       src/batch_processor.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Executables
TARGETS = fast-pdf-parser simple-test stream-test perf-test limited-test fast-output-test docling-compatible token-test hierarchical-docling simple-hierarchical hierarchical-refactored hierarchical-enhanced hierarchical-final benchmark-passes tokenizer-example

# Default target
all: $(TARGETS)

# Build library object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Main CLI
fast-pdf-parser: $(OBJS) src/main.o
	$(CXX) -o $@ $^ $(LDFLAGS)

# Test programs
simple-test: $(OBJS) src/simple_test.o
	$(CXX) -o $@ $^ $(LDFLAGS)

stream-test: $(OBJS) src/stream_test.o
	$(CXX) -o $@ $^ $(LDFLAGS)

perf-test: $(OBJS) src/perf_test.o
	$(CXX) -o $@ $^ $(LDFLAGS)

limited-test: $(OBJS) src/limited_test.o
	$(CXX) -o $@ $^ $(LDFLAGS)

fast-output-test: $(OBJS) src/fast_output_test.o
	$(CXX) -o $@ $^ $(LDFLAGS)

docling-compatible: $(OBJS) src/docling_compatible.o
	$(CXX) -o $@ $^ $(LDFLAGS)

token-test: src/token_test.o
	$(CXX) -o $@ $^ $(LDFLAGS)

hierarchical-docling: $(OBJS) src/hierarchical_docling.o
	$(CXX) -o $@ $^ $(LDFLAGS)

simple-hierarchical: $(OBJS) src/simple_hierarchical.o
	$(CXX) -o $@ $^ $(LDFLAGS)

hierarchical-refactored: $(OBJS) src/hierarchical_refactored.o
	$(CXX) -o $@ $^ $(LDFLAGS)

hierarchical-enhanced: $(OBJS) src/hierarchical_enhanced.o
	$(CXX) -o $@ $^ $(LDFLAGS)

hierarchical-final: src/hierarchical_final.o
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
test: limited-test
	./limited-test n3797.pdf

.PHONY: all clean test