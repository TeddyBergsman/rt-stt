#!/bin/bash

echo "RT-STT Configuration Management Test"
echo "===================================="
echo ""

# Check if daemon is running
if ! pgrep -f "rt-stt.*--socket.*test" > /dev/null; then
    echo "Starting test daemon first..."
    ./test_daemon_local.sh &
    sleep 3
fi

SOCKET="/tmp/rt-stt-test.sock"

echo "1. Getting current configuration..."
echo "-----------------------------------"
./build/rt-stt-cli get-config -s $SOCKET

echo ""
echo "2. Getting performance metrics..."
echo "---------------------------------"
./build/rt-stt-cli get-metrics -s $SOCKET

echo ""
echo "3. Testing VAD sensitivity change..."
echo "------------------------------------"
echo "Setting VAD sensitivity to 1.2..."
rt-stt-cli set-vad-sensitivity 1.2 -s $SOCKET

echo ""
echo "4. Testing language change..."
echo "-----------------------------"
echo "Setting language to Spanish..."
rt-stt-cli set-language es -s $SOCKET

echo ""
echo "Checking status after changes..."
rt-stt-cli status -s $SOCKET

echo ""
echo "5. Testing comprehensive VAD config update..."
echo "---------------------------------------------"
rt-stt-cli set-vad \
    --start-threshold 1.1 \
    --end-threshold 0.9 \
    --speech-start-ms 200 \
    --speech-end-ms 800 \
    -s $SOCKET

echo ""
echo "6. Testing full config update..."
echo "--------------------------------"
echo "Creating a test configuration..."
cat > /tmp/test-vad-config.json << EOF
{
    "vad_config": {
        "speech_start_threshold": 1.05,
        "speech_end_threshold": 0.88,
        "min_speech_ms": 400
    }
}
EOF

echo "Applying configuration..."
rt-stt-cli set-config "$(cat /tmp/test-vad-config.json)" -s $SOCKET

echo ""
echo "7. Getting updated config as JSON..."
echo "------------------------------------"
rt-stt-cli get-config -j -s $SOCKET | jq '.vad_config | {speech_start_threshold, speech_end_threshold, min_speech_ms}'

echo ""
echo "8. Testing model switching..."
echo "-----------------------------"
if [ -f "models/ggml-base.en.bin" ]; then
    echo "Switching to base.en model..."
    rt-stt-cli set-model models/ggml-base.en.bin -s $SOCKET
    sleep 2
    echo "Switching back to small.en model..."
    rt-stt-cli set-model models/ggml-small.en.bin -s $SOCKET
else
    echo "base.en model not found, skipping model switch test"
fi

echo ""
echo "Configuration management test complete!"
echo ""
echo "Tips:"
echo "- Use 'rt-stt-cli get-config -j' for JSON output"
echo "- Use 'rt-stt-cli set-vad --help' for all VAD options" 
echo "- Configuration changes are applied immediately"
echo "- Most changes don't require daemon restart"
