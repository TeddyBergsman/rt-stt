#!/bin/bash

echo "RT-STT with Improved Sensitivity"
echo "================================"
echo ""
echo "✅ Fixed: Much more sensitive VAD (1.05x noise floor)"
echo "✅ Fixed: Pre-speech buffer captures first words"
echo "✅ Fixed: Faster noise adaptation"
echo ""
echo "You should now be able to:"
echo "- Speak at normal volume (no shouting!)"
echo "- Have the first word captured correctly"
echo ""
echo "Test phrases:"
echo "1. 'Testing one two three four five'"
echo "2. 'Hello my name is Teddy Bergsman'"
echo ""
echo "The pre-speech buffer will show:"
echo "'Added pre-speech buffer: 0.3 seconds'"
echo ""

if [ -f "models/ggml-small.en.bin" ]; then
    ./build/rt-stt-test --model models/ggml-small.en.bin "$@"
else
    echo "⚠️  Small model not found, using base model"
    ./build/rt-stt-test --model models/ggml-base.en.bin "$@"
fi
