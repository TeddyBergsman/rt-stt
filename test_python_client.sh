#!/bin/bash

echo "RT-STT Python Client Test"
echo "========================"
echo ""
echo "This will connect to the RT-STT daemon using the Python client."
echo "Make sure the daemon is running first (in another terminal)."
echo ""

SOCKET_PATH="/tmp/rt-stt-test.sock"

# Check if socket exists
if [ ! -S "$SOCKET_PATH" ]; then
    echo "⚠️  Socket not found at $SOCKET_PATH"
    echo ""
    echo "Please start the daemon first in another terminal:"
    echo "  ./test_ipc.sh"
    echo ""
    exit 1
fi

echo "Connecting to RT-STT daemon at $SOCKET_PATH..."
echo ""

# Run Python client with custom socket path
python3 examples/python_client.py "$SOCKET_PATH"
