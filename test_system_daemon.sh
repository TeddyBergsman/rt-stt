#!/bin/bash

echo "RT-STT System Daemon Test"
echo "========================="
echo ""
echo "This test will verify the system daemon has the latest features."
echo ""

# Test 1: Basic streaming
echo "1. Testing basic streaming (5 seconds)..."
echo "   Speak something to test transcription:"
echo ""
timeout 5 rt-stt-cli stream || true

echo ""
echo "2. Testing JSON output with full metadata..."
echo "   Speak again to see complete metadata:"
echo ""
echo "Streaming for 5 seconds..."
timeout 5 rt-stt-cli stream -j | while read -r line; do
    echo "$line" | jq '.' 2>/dev/null || echo "$line"
done || true

echo ""
echo "3. Checking configuration..."
python3 -m rt_stt.cli get-config -j | jq '{
  model: .model_config.model_path,
  language: .model_config.language,
  vad_enabled: .vad_config.use_adaptive_threshold,
  socket: .ipc_socket_path
}' 2>/dev/null || echo "Python CLI not available, using basic status"

echo ""
echo "4. Performance metrics..."
python3 -m rt_stt.cli get-metrics 2>/dev/null || echo "Metrics not available yet"

echo ""
echo "5. Daemon details..."
echo "   PID: $(launchctl list | grep rt-stt | awk '{print $1}')"
echo "   Socket: $(ls -la /tmp/rt-stt.sock 2>/dev/null || echo "Socket not found at /tmp/rt-stt.sock")"
echo "   Binary: $(ls -la /usr/local/bin/rt-stt | awk '{print $5, $6, $7, $8}')"
echo "   Logs: ~/Library/Logs/rt-stt.log"

echo ""
echo "Test complete! Your daemon is running with:"
echo "- Default socket: /tmp/rt-stt.sock"
echo "- Config: ~/Library/Application Support/rt-stt/config.json"
echo "- Models: /usr/local/share/rt-stt/models/"
