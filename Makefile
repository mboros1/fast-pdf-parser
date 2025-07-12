# Fast PDF Parser Makefile

CXX = clang++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -pthread \
           -I/opt/homebrew/include \
           -I/opt/homebrew/Cellar/mupdf-tools/1.26.3/include \
           -Iinclude

LDFLAGS = -L/opt/homebrew/lib \
          -L/opt/homebrew/Cellar/mupdf-tools/1.26.3/lib \
          -lmupdf -lmupdf-third -lz -pthread

# Core PDF parsing files (used by hierarchical-chunker)
CORE_SRCS = src/fast_pdf_parser.cpp \
            src/thread_pool.cpp \
            src/text_extractor.cpp

# Additional files for original fast-pdf-parser
PARSER_SRCS = src/json_serializer.cpp \
              src/batch_processor.cpp

# Object files
CORE_OBJS = $(CORE_SRCS:.cpp=.o)
PARSER_OBJS = $(PARSER_SRCS:.cpp=.o)
ALL_OBJS = $(CORE_OBJS) $(PARSER_OBJS)

# Executables
TARGETS = fast-pdf-parser perf-test token-test hierarchical-chunker benchmark-passes tokenizer-example

# Default target
all: $(TARGETS)

# Build library object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Main CLI (uses nlohmann json)
fast-pdf-parser: $(ALL_OBJS) src/main.o
	$(CXX) -o $@ $^ $(LDFLAGS)

# Test programs
perf-test: $(CORE_OBJS) src/perf_test.o
	$(CXX) -o $@ $^ $(LDFLAGS)

token-test: src/token_test.o
	$(CXX) -o $@ $^ $(LDFLAGS)

# Our main implementation (uses rapidjson)
hierarchical-chunker: $(CORE_OBJS) src/hierarchical_chunker.o
	$(CXX) -o $@ $^ $(LDFLAGS)

benchmark-passes: benchmarks/benchmark_passes.o
	$(CXX) -o $@ $^ $(LDFLAGS)

tokenizer-example: examples/tokenizer_example.o
	$(CXX) -o $@ $^ $(LDFLAGS)

# Clean
clean:
	rm -f $(ALL_OBJS) src/*.o benchmarks/*.o examples/*.o tests/*.o $(TARGETS)
	rm -rf out/
	rm -f *.cmake *.sh
	rm -f cl100k_base.tiktoken

# Run test
test: hierarchical-chunker
	./hierarchical-chunker n3797.pdf 512 50 100

.PHONY: all clean test