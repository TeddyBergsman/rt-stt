#!/bin/bash

echo "RT-STT Complete System Inspection"
echo "================================="
echo ""

PYTHON_CLI="python3 -m rt_stt.cli"
SOCKET="/tmp/rt-stt-test.sock"

echo "1. Full Configuration (All Settings)"
echo "------------------------------------"
$PYTHON_CLI -s $SOCKET get-config -j | jq '.'

echo ""
echo "2. Performance Metrics"
echo "----------------------"
$PYTHON_CLI -s $SOCKET get-metrics -j | jq '.'

echo ""
echo "3. Current Status"
echo "-----------------"
$PYTHON_CLI -s $SOCKET status

echo ""
echo "4. Streaming with Full JSON Output"
echo "----------------------------------"
echo "Speak now to see complete transcription data..."
echo "(Press Ctrl+C to stop)"
echo ""

# Stream for 10 seconds showing full JSON
timeout 10 $PYTHON_CLI -s $SOCKET stream -j | while read -r line; do
    echo "$line" | jq '.'
    echo "---"
done

echo ""
echo "5. Raw Configuration File"
echo "-------------------------"
if [ -f "config/local_test_config.json" ]; then
    echo "Local test config:"
    cat config/local_test_config.json | jq '.'
fi
