#!/bin/bash

echo "RT-STT Working Version!"
echo "======================"
echo ""
echo "✅ FIXED: VAD state transition detection"
echo "✅ FIXED: More sensitive speech detection (1.2x instead of 1.5x)"
echo "✅ FIXED: Now correctly detects SPEECH_ENDING -> SILENCE"
echo ""
echo "What to expect:"
echo "1. Say: 'Testing one two three four five'"
echo "2. Stop speaking and wait ~1 second"
echo "3. You should see:"
echo "   - 'VAD: SPEECH_ENDING -> SILENCE transition detected'"
echo "   - 'Processing utterance: X.X seconds'"
echo "   - Then your transcription!"
echo ""
echo "Using small.en model..."
echo ""

if [ -f "models/ggml-small.en.bin" ]; then
    ./build/rt-stt-test --model models/ggml-small.en.bin "$@"
else
    echo "⚠️  Small model not found, using base model"
    ./build/rt-stt-test --model models/ggml-base.en.bin "$@"
fi
