#!/bin/bash

# Download better Whisper models for improved accuracy

set -e

echo "Downloading better Whisper models..."
echo "===================================="

# Create models directory
mkdir -p models

# Base URL
BASE_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main"

# Download small.en model (recommended for better accuracy)
if [ -f "models/ggml-small.en.bin" ]; then
    echo "Small English model already exists"
else
    echo "Downloading small.en model (466 MB - better accuracy)..."
    curl -L "${BASE_URL}/ggml-small.en.bin" -o "models/ggml-small.en.bin" --progress-bar
fi

# Download medium.en model (best accuracy while maintaining speed)
if [ -f "models/ggml-medium.en.bin" ]; then
    echo "Medium English model already exists"
else
    echo "Downloading medium.en model (1.5 GB - best accuracy for real-time)..."
    curl -L "${BASE_URL}/ggml-medium.en.bin" -o "models/ggml-medium.en.bin" --progress-bar
fi

echo ""
echo "✅ Download complete!"
echo ""
echo "To test with better models:"
echo "  Small model:  ./test_clean.sh --model models/ggml-small.en.bin"
echo "  Medium model: ./test_clean.sh --model models/ggml-medium.en.bin"
echo ""
echo "The small model should give much better accuracy with reasonable latency."
echo "The medium model gives the best accuracy but may have higher latency."
