#!/bin/bash

echo "RT-STT Input Channel Selection Test"
echo "==================================="
echo ""
echo "Testing with Input 1 only from MOTU M2"
echo ""
echo "The application will:"
echo "1. Display available input channels"
echo "2. Select only Input 1 (channel 0)"
echo "3. Ignore Input 2 and any other channels"
echo ""

if [ -f "models/ggml-small.en.bin" ]; then
    ./build/rt-stt-test --model models/ggml-small.en.bin "$@"
else
    echo "⚠️  Small model not found, using base model"
    ./build/rt-stt-test --model models/ggml-base.en.bin "$@"
fi
