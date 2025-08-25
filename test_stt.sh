#!/bin/bash

# RT-STT Test Script
# This script runs the STT application with the recommended settings

set -e

echo "RT-STT Test Script"
echo "=================="
echo ""

# Check if build exists
if [ ! -f "./build/rt-stt-test" ]; then
    echo "Error: Application not built. Please run ./build.sh first"
    exit 1
fi

# Check if model exists
MODEL="models/ggml-base.en.bin"
if [ ! -f "$MODEL" ]; then
    echo "Error: Model not found at $MODEL"
    echo "Please run ./scripts/download_models.sh and select option 2"
    exit 1
fi

echo "Starting RT-STT with base.en model..."
echo "Using default audio device (pass --device \"MOTU M2\" to use MOTU M2)"
echo ""
echo "Press Ctrl+C to stop"
echo ""

# Run the application
./build/rt-stt-test --model "$MODEL" "$@"
