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

echo ""
echo "Building all system components..."
echo "================================="

# 1. Build main ASCII structure system
echo "1. Compiling main ASCII structure system..."
gcc -o ascii_structure_system main.c constraint_solver.c \
    constraints.c tree_debug.c llm_integration.c \
    $(pkg-config --cflags --libs libcurl libcjson) \
    -lm -Wall -Wextra

if [ $? -ne 0 ]; then
    echo "❌ Main system build failed!"
    exit 1
fi

# 2. Build constraint testing system
echo "2. Compiling constraint testing system..."
gcc -o constraint_test constraint_test.c constraint_solver.c constraints.c tree_debug.c \
    -lm -Wall -Wextra

if [ $? -ne 0 ]; then
    echo "❌ Constraint test system build failed!"
    exit 1
fi


echo ""
echo "✅ All builds successful!"
echo "========================="
echo "Built components:"
echo "  • ascii_structure_system  - Main constraint solver system"
echo "  • constraint_test         - Constraint testing and visualization"
echo ""
echo "Usage:"
echo "  ./ascii_structure_system  - Run main system (requires OpenAI API key)"
echo "  ./constraint_test         - Test individual constraints interactively"
echo ""
echo "For main system, set your OpenAI API key:"
echo "export OPENAI_API_KEY='your-api-key-here'"