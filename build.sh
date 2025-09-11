#!/bin/bash

echo "Building ASCII Structure System..."

# Check for required dependencies
echo "Checking dependencies..."

# Check for libcurl
if ! pkg-config --exists libcurl; then
    echo "Error: libcurl not found. Install with: sudo apt-get install libcurl4-openssl-dev"
    exit 1
fi

# Check for cjson
if ! pkg-config --exists libcjson; then
    echo "Error: libcjson not found. Install with: sudo apt-get install libcjson-dev"
    exit 1
fi

# Compile the system
echo "Compiling modular ASCII structure system..."
gcc -o ascii_structure_system main.c constraint_solver.c llm_integration.c \
    $(pkg-config --cflags --libs libcurl libcjson) \
    -lm -Wall -Wextra

if [ $? -eq 0 ]; then
    echo "✅ Build successful!"
    echo "Files compiled: main.c, constraint_solver.c, llm_integration.c"
    echo "Run with: ./ascii_structure_system"
    echo ""
    echo "Make sure to set your OpenAI API key:"
    echo "export OPENAI_API_KEY='your-api-key-here'"
else
    echo "❌ Build failed!"
    exit 1
fi