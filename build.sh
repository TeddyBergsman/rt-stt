#!/bin/bash

# Build script for rt-stt

set -e

echo "RT-STT Build Script"
echo "=================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if dependencies exist
if [ ! -f "third_party/miniaudio/miniaudio.h" ] || [ ! -d "third_party/whisper.cpp" ]; then
    echo -e "${YELLOW}Dependencies not found. Running download script...${NC}"
    ./scripts/download_dependencies.sh
fi

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo -e "${GREEN}Configuring with CMake...${NC}"
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DWHISPER_METAL=ON \
    -DWHISPER_COREML=ON \
    -DWHISPER_ACCELERATE=ON

# Build
echo -e "${GREEN}Building...${NC}"
make -j$(sysctl -n hw.ncpu)

echo -e "${GREEN}Build complete!${NC}"
echo ""
echo "Executables:"
echo "  - build/rt-stt-test (test application with terminal output)"
echo "  - build/rt-stt (main daemon)"
echo ""
echo "To test the application:"
echo "  1. First download a model: ./scripts/download_models.sh"
echo "  2. Run: ./build/rt-stt-test --model models/ggml-base.en.bin"
echo ""
echo "For help: ./build/rt-stt-test --help"
