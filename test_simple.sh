#!/bin/bash

echo "RT-STT Simple Test"
echo "=================="
echo ""
echo "Testing with simplified VAD settings..."
echo ""
echo "Instructions:"
echo "1. Say: 'Testing one two three four five'"
echo "2. PAUSE for 1 full second"
echo "3. You should see the transcription"
echo ""
echo "If it doesn't work, the VAD is the problem."
echo ""

# Disable adaptive threshold for more predictable behavior
./build/rt-stt-test --model models/ggml-base.en.bin "$@"
