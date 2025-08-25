#!/bin/bash

echo "RT-STT Pre-Speech Buffer Debug Test"
echo "==================================="
echo ""
echo "Improvements:"
echo "✅ Pre-speech buffer increased to 500ms"
echo "✅ More hysteresis in VAD (1.08x/0.85x)"
echo "✅ Longer silence required (1 second)"
echo "✅ Debug output for buffer contents"
echo ""
echo "You should see:"
echo "1. 'Starting new utterance - cleared speech buffer'"
echo "2. 'Pre-speech buffer: 0.5 seconds, max amplitude: X.X'"
echo "3. 'First 0.5s max amplitude: X.X' (should be non-zero)"
echo ""
echo "Test phrase: 'Testing one two three four five'"
echo ""

if [ -f "models/ggml-small.en.bin" ]; then
    ./build/rt-stt-test --model models/ggml-small.en.bin "$@"
else
    echo "⚠️  Small model not found, using base model"
    ./build/rt-stt-test --model models/ggml-base.en.bin "$@"
fi
