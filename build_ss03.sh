#!/bin/bash
# Build script for SS03Game module

echo "======================================"
echo "Building Monte Carlo Simulator"
echo "Game Module: SS03Game"
echo "======================================"

# Configure with CMake
cmake -B build \
    -DGAME_MODULE=SS03Game \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=/opt/homebrew/bin/g++-15

# Build
cmake --build build -j 10

echo ""
echo "Build complete! Run with: ./build/simulator"
