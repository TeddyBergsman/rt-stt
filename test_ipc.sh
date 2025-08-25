#!/bin/bash

echo "RT-STT IPC Server Test"
echo "====================="
echo ""
echo "This will start the RT-STT daemon with IPC server enabled."
echo "You can then connect using the Python client example."
echo ""

# Check if models exist
if [ ! -f "models/ggml-small.en.bin" ]; then
    echo "⚠️  Small model not found. Please download models first."
    exit 1
fi

echo "Starting RT-STT daemon..."
echo ""

# Run the daemon (not as background so we can see output)
./build/rt-stt --socket /tmp/rt-stt-test.sock
