#!/bin/bash

echo "RT-STT Config Loading Test"
echo "=========================="
echo ""

# Create a test config
cat > /tmp/test-config.json << 'EOF'
{
  "stt": {
    "model": {
      "path": "models/ggml-small.en.bin",
      "language": "en"
    },
    "audio": {
      "device_name": "TEST DEVICE",
      "input_channel_index": 0
    }
  },
  "ipc": {
    "socket_path": "/tmp/rt-stt-config-test.sock"
  }
}
EOF

echo "Test config created at /tmp/test-config.json"
echo ""
echo "Starting daemon with test config..."
echo "You should see:"
echo "  - 'Loaded configuration from: /tmp/test-config.json'"
echo "  - 'Audio device: TEST DEVICE (Input 1)'"
echo "  - 'Listening on: /tmp/rt-stt-config-test.sock'"
echo ""

./build/rt-stt --config /tmp/test-config.json
