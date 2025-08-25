#!/bin/bash

# Use the Python CLI with full path
PYTHON_CLI="$HOME/Library/Python/3.13/bin/rt-stt-cli"
SOCKET="/tmp/rt-stt-test.sock"

echo "Switching to large multilingual model..."
echo ""

# Check if large model exists
if [ ! -f "models/ggml-large-v3.bin" ]; then
    echo "Error: Large model not found at models/ggml-large-v3.bin"
    echo "Please download it first with: ./scripts/download_models.sh"
    exit 1
fi

# Switch model
echo "Setting model to large-v3..."
$PYTHON_CLI -s $SOCKET set-model models/ggml-large-v3.bin

# Enable auto language detection
echo "Enabling auto language detection..."
$PYTHON_CLI -s $SOCKET set-language auto

echo ""
echo "Configuration updated! Current status:"
$PYTHON_CLI -s $SOCKET status

echo ""
echo "The model now supports 99 languages with auto-detection!"
echo "Try speaking in different languages."
