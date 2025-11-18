#!/bin/bash

# Quick build and run script
# Usage: ./build.sh

set -e  # Exit on error

echo "================================================"
echo "  Building Synth-C"
echo "================================================"
echo ""

# Check if we have a build directory
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

# Build
cd build
echo "Running CMake..."
cmake ..
echo ""
echo "Compiling..."
make
echo ""

echo "================================================"
echo "  Build Complete!"
echo "================================================"
echo ""
echo "Run with: ./build/synth"
echo ""

# Ask if user wants to run
read -p "Run now? (y/n) " -n 1 -r
echo ""
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Running synth..."
    echo ""
    ./synth
fi
