#!/bin/bash

echo "RT-STT Daemon Local Test"
echo "========================"
echo ""
echo "This tests the daemon in foreground mode before installation."
echo ""

# Check if config exists locally
if [ -f "config/local_test_config.json" ]; then
    echo "Using local test config file..."
    CONFIG_PATH="config/local_test_config.json"
elif [ -f "config/default_config.json" ]; then
    echo "Using default config file (WARNING: may have system paths)..."
    CONFIG_PATH="config/default_config.json"
else
    echo "Creating temporary config..."
    CONFIG_PATH="/tmp/rt-stt-test-config.json"
    cat > "$CONFIG_PATH" << 'EOF'
{
  "stt": {
    "model": {
      "path": "models/ggml-small.en.bin",
      "language": "en",
      "use_gpu": true
    },
    "vad": {
      "enabled": true,
      "speech_start_threshold": 1.08,
      "speech_end_threshold": 0.85,
      "pre_speech_buffer_ms": 500
    },
    "audio": {
      "device_name": "MOTU M2",
      "input_channel_index": 1,
      "force_single_channel": true
    }
  },
  "ipc": {
    "socket_path": "/tmp/rt-stt-test.sock"
  }
}
EOF
fi

echo "Starting daemon with config: $CONFIG_PATH"
echo "Socket: /tmp/rt-stt-test.sock"
echo ""

# Run daemon in foreground
./build/rt-stt --config "$CONFIG_PATH" --socket /tmp/rt-stt-test.sock
