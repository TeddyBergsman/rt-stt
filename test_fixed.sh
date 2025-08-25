#!/bin/bash

echo "RT-STT Fixed Version Test"
echo "========================"
echo ""
echo "Major fixes applied:"
echo "✅ Process complete sentences only (no fragments)"
echo "✅ Wait for speech to end before processing"
echo "✅ No sliding windows (eliminates repetition)"
echo "✅ Better VAD thresholds (800ms silence = sentence end)"
echo "✅ Minimum utterance length (0.5 seconds)"
echo ""
echo "Expected behavior:"
echo "- Speak a complete sentence"
echo "- Wait for brief pause"
echo "- See ONE transcription of the full sentence"
echo "- Latency should be <1 second after you stop speaking"
echo ""
echo "Test phrase: 'Testing one two three four five.'"
echo "Then pause, then: 'Hello my name is Teddy Bergsman.'"
echo ""
echo "Using small.en model for better accuracy..."
echo ""

if [ -f "models/ggml-small.en.bin" ]; then
    ./build/rt-stt-test --model models/ggml-small.en.bin "$@"
else
    echo "⚠️  Small model not found, using base model"
    ./build/rt-stt-test --model models/ggml-base.en.bin "$@"
fi
