#!/bin/bash

# Debug script for RT-STT
# This runs the STT with extensive debug output

echo "RT-STT Debug Mode"
echo "================="
echo ""
echo "This version includes extensive debug output to trace the audio pipeline:"
echo "- Audio sample statistics (max/avg amplitude)"
echo "- VAD state transitions"
echo "- Speech buffer growth"
echo "- Processing thread activity"
echo "- Whisper processing details"
echo ""
echo "Try these tests:"
echo "1. First run WITH VAD disabled to force processing:"
echo "   ./debug_stt.sh --no-vad"
echo ""
echo "2. Then run with normal VAD:"
echo "   ./debug_stt.sh"
echo ""
echo "Speak clearly and watch for debug output..."
echo "Press Ctrl+C to stop"
echo ""

./build/rt-stt-test --model models/ggml-base.en.bin "$@"
