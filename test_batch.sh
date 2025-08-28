#!/bin/bash

# Simple batch test script for the new CLI
echo "Testing batch PDF processing with new CLI..."

# Test with a few PDFs
PDF_DIR="~/git/native-vector-store/samples/pdf"
OUTPUT_DIR="./batch_test_output"

mkdir -p "$OUTPUT_DIR"

# Process first 3 PDFs as a test
for pdf in $(ls $PDF_DIR/*.pdf | head -3); do
    basename=$(basename "$pdf")
    output="$OUTPUT_DIR/${basename%.pdf}_chunks.json"
    
    echo "Processing: $basename"
    ./chunk-pdf-cli \
        --input "$pdf" \
        --output "$output" \
        --max-chunk-size 1000 \
        --min-chunk-size 100 \
        --overlap 50 \
        --quiet
    
    if [ $? -eq 0 ]; then
        echo "  ✓ Success"
    else
        echo "  ✗ Failed"
    fi
done

echo ""
echo "Results in: $OUTPUT_DIR"
ls -lh "$OUTPUT_DIR"