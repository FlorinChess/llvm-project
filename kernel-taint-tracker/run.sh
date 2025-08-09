#!/bin/bash

# Usage: ./run.sh

OUT_DIR="./llvm-testcases"

# Create output directory if it doesn't exist
mkdir -p "$OUT_DIR"

# Loop through each .c file in the source directory
for src_file in ./c-testcases/*; do
    
    # Get the filename without the path and extension
    filename="${src_file##*/}"
    OUT_FILE="${filename}.ll" 

    # Compile the file with clang
    clang -S -emit-llvm "${src_file}" -o "${OUT_DIR}"/"${OUT_FILE}"

    # Check if compilation succeeded
    if [ $? -eq 0 ]; then
        echo "Compiled $src_file -> $OUT_DIR/$OUT_FILE"
    else
        echo "Failed to compile $src_file"
    fi
done
