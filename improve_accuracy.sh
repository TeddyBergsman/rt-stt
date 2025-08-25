#!/bin/bash

# Script to improve STT accuracy with better settings

echo "RT-STT Accuracy Improvements"
echo "==========================="
echo ""
echo "This script runs the STT with optimized settings for better accuracy."
echo ""

# Check if better models exist
if [ -f "models/ggml-small.en.bin" ]; then
    echo "✅ Using small.en model for better accuracy"
    MODEL="models/ggml-small.en.bin"
elif [ -f "models/ggml-base.en.bin" ]; then
    echo "⚠️  Using base.en model (download small.en for better results)"
    MODEL="models/ggml-base.en.bin"
else
    echo "❌ No models found! Run ./scripts/download_models.sh first"
    exit 1
fi

echo ""
echo "Tips for best results:"
echo "- Speak clearly and at a moderate pace"
echo "- Keep your microphone 6-12 inches from your mouth"
echo "- Minimize background noise"
echo "- Try: 'Hello, my name is [your name] and I'm testing one two three four five.'"
echo ""
echo "Press Ctrl+C to stop"
echo ""

# Run with optimized settings
./build/rt-stt-test --model "$MODEL" "$@"
