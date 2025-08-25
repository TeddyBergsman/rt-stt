#!/bin/bash

echo "Resetting RT-STT to default configuration..."
echo ""

# Use the Python CLI with full path
PYTHON_CLI="$HOME/Library/Python/3.13/bin/rt-stt-cli"
SOCKET="/tmp/rt-stt-test.sock"

# Reset to small.en model
echo "Setting model back to small.en..."
$PYTHON_CLI -s $SOCKET set-model models/ggml-small.en.bin

# Reset to English only
echo "Setting language back to English..."
$PYTHON_CLI -s $SOCKET set-language en

# Reset VAD to defaults
echo "Resetting VAD settings..."
$PYTHON_CLI -s $SOCKET set-vad \
    --start-threshold 1.08 \
    --end-threshold 0.85 \
    --speech-start-ms 150 \
    --speech-end-ms 1000 \
    --min-speech-ms 500

echo ""
echo "Configuration reset to defaults!"
echo ""
echo "Current status:"
$PYTHON_CLI -s $SOCKET status
