#!/bin/bash

# Clean test script for RT-STT
# This runs the STT with minimal debug output

echo "RT-STT Clean Test"
echo "================="
echo ""
echo "Improved version with:"
echo "- Fixed VAD thresholds"
echo "- Better streaming parameters"
echo "- Cleaner output"
echo ""
echo "Speak clearly into your microphone..."
echo "Numbers work well: 'one, two, three, four, five'"
echo ""
echo "Press Ctrl+C to stop"
echo ""

./build/rt-stt-test --model models/ggml-base.en.bin "$@"
