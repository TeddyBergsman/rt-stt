#!/bin/bash

echo "RT-STT Output Comparison"
echo "========================"
echo ""
echo "This will show the same transcription in different formats."
echo "Speak once, then wait to see all formats..."
echo ""

SOCKET="/tmp/rt-stt-test.sock"

# Create a named pipe
PIPE=$(mktemp -u)
mkfifo $PIPE

# Start JSON capture in background
timeout 5 rt-stt-cli stream -j -s $SOCKET > $PIPE &

# Read one transcription
JSON_OUTPUT=$(head -n 1 $PIPE)

# Clean up
rm -f $PIPE

# Display results
echo "1. NORMAL OUTPUT (what you usually see):"
echo "----------------------------------------"
echo "$JSON_OUTPUT" | jq -r '.data.text'

echo ""
echo "2. JSON OUTPUT (raw):"
echo "--------------------"
echo "$JSON_OUTPUT"

echo ""
echo "3. JSON OUTPUT (formatted):"
echo "--------------------------"
echo "$JSON_OUTPUT" | jq '.'

echo ""
echo "4. EXTRACTED METADATA:"
echo "---------------------"
echo "$JSON_OUTPUT" | jq '{
  text: .data.text,
  confidence: .data.confidence,
  language: .data.language,
  processing_time_ms: .data.processing_time_ms,
  timestamp: .data.timestamp
}'

echo ""
echo "5. PERFORMANCE DATA:"
echo "-------------------"
echo "$JSON_OUTPUT" | jq '.data | {
  processing_time_ms,
  duration_ms,
  model,
  language_probability
}'
