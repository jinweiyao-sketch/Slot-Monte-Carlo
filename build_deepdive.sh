#!/bin/bash
# Build script for DeepDive module

echo "======================================"
echo "Building Monte Carlo Simulator"
echo "Game Module: DeepDive"
echo "======================================"

# Configure with CMake
cmake -B build \
    -DGAME_MODULE=DeepDive \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=/opt/homebrew/bin/g++-15

# Build
cmake --build build -j 10

echo ""
echo "Build complete! Run with: ./build/simulator"
