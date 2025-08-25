#!/bin/bash

echo "RT-STT VAD Tuning Helper"
echo "========================"
echo ""
echo "This script helps you find the right VAD settings for your environment."
echo ""
echo "Current settings:"
echo "- Speech threshold: 1.05x noise floor (very sensitive)"
echo "- Pre-speech buffer: 300ms"
echo ""
echo "If you still need to speak loudly, try running with --no-vad"
echo "to confirm audio input is working, then adjust thresholds."
echo ""
echo "Tips:"
echo "1. In a quiet room, 1.05x should work well"
echo "2. In a noisy environment, try 1.2x-1.5x"
echo "3. Check your mic gain in System Settings > Sound"
echo "4. Position mic 6-12 inches from mouth"
echo ""
echo "Starting with current settings..."
echo ""

# Show current noise floor in debug
export RUST_LOG=debug

if [ -f "models/ggml-small.en.bin" ]; then
    ./build/rt-stt-test --model models/ggml-small.en.bin "$@"
else
    ./build/rt-stt-test --model models/ggml-base.en.bin "$@"
fi
