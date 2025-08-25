#!/bin/bash

echo "RT-STT IPC Server Test (Verbose)"
echo "================================"
echo ""
echo "This will start the RT-STT daemon with:"
echo "✓ IPC server on /tmp/rt-stt-test.sock"
echo "✓ Audio capture from MOTU M2 (Input 2)"
echo "✓ Real-time transcription broadcasting"
echo ""

# Check if models exist
if [ ! -f "models/ggml-small.en.bin" ]; then
    echo "⚠️  Small model not found. Please download models first."
    exit 1
fi

echo "Starting RT-STT daemon..."
echo ""
echo "You should see:"
echo "1. Audio device initialization"
echo "2. IPC server starting"
echo "3. 'Listening on: /tmp/rt-stt-test.sock'"
echo ""
echo "Then in another terminal, run:"
echo "  ./test_python_client.sh"
echo ""
echo "============================================"
echo ""

# Run the daemon (not as background so we can see output)
./build/rt-stt --socket /tmp/rt-stt-test.sock
