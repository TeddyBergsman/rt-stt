#!/bin/bash

# Script to download third-party dependencies

set -e

echo "Downloading dependencies..."

# Change to project root
cd "$(dirname "$0")/.."

# Download miniaudio
echo "Downloading miniaudio..."
curl -L https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h -o third_party/miniaudio/miniaudio.h

# Download nlohmann/json
echo "Downloading nlohmann/json..."
curl -L https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp -o third_party/json/include/nlohmann/json.hpp

# Clone whisper.cpp
echo "Cloning whisper.cpp..."
if [ -d "third_party/whisper.cpp" ]; then
    echo "whisper.cpp already exists, updating..."
    cd third_party/whisper.cpp
    git pull
    cd ../..
else
    git clone https://github.com/ggerganov/whisper.cpp.git third_party/whisper.cpp
fi

# Build whisper.cpp with Core ML support
echo "Building whisper.cpp with Core ML support..."
cd third_party/whisper.cpp

# Build whisper.cpp
if [ -f "Makefile" ]; then
    # Clean if already built
    if [ -f "whisper.o" ]; then
        make clean
    fi
    
    # Build with Core ML support
    WHISPER_COREML=1 WHISPER_METAL=1 make -j8
else
    echo "Error: Makefile not found in whisper.cpp directory"
    exit 1
fi

echo "Dependencies downloaded successfully!"

# Download models
echo ""
echo "Would you like to download Whisper models? (y/n)"
read -r response
if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
    cd "$OLDPWD"
    ./scripts/download_models.sh
fi
