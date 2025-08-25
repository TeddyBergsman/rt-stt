#!/bin/bash

echo "RT-STT Input 2 Channel Selection Test"
echo "====================================="
echo ""
echo "Testing with Input 2 only from MOTU M2"
echo ""
echo "The application will:"
echo "1. Display available input channels"
echo "2. Select only Input 2 (channel 1)"
echo "3. Ignore Input 1 and any other channels"
echo ""

if [ -f "models/ggml-small.en.bin" ]; then
    ./build/rt-stt-test --model models/ggml-small.en.bin "$@"
else
    echo "⚠️  Small model not found, using base model"
    ./build/rt-stt-test --model models/ggml-base.en.bin "$@"
fi
