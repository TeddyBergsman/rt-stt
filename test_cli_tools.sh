#!/bin/bash

echo "RT-STT CLI Tools Test"
echo "===================="
echo ""

# Check if daemon is running
echo "1. Testing C++ CLI tool..."
echo "--------------------------"

# Build if needed
if [ ! -f "build/rt-stt-cli" ]; then
    echo "Building CLI tool..."
    cd build && make rt-stt-cli && cd ..
fi

echo ""
echo "Testing status command:"
./build/rt-stt-cli status -s /tmp/rt-stt-test.sock

echo ""
echo "Available commands:"
./build/rt-stt-cli --help

echo ""
echo "2. Testing Python CLI..."
echo "------------------------"

# Install Python package in development mode
echo "Installing Python package..."
cd python && pip3 install -e . --user && cd ..

echo ""
echo "Testing Python CLI status:"
rt-stt-cli status -s /tmp/rt-stt-test.sock

echo ""
echo "Python CLI help:"
rt-stt-cli --help

echo ""
echo "3. Quick Tests"
echo "--------------"
echo ""
echo "To stream transcriptions:"
echo "  C++: ./build/rt-stt-cli stream -s /tmp/rt-stt-test.sock"
echo "  Python: rt-stt-cli stream -s /tmp/rt-stt-test.sock"
echo ""
echo "To stream with timestamps:"
echo "  C++: ./build/rt-stt-cli stream -t -s /tmp/rt-stt-test.sock"
echo "  Python: rt-stt-cli stream -t -s /tmp/rt-stt-test.sock"
echo ""
echo "To stream as JSON:"
echo "  C++: ./build/rt-stt-cli stream -j -s /tmp/rt-stt-test.sock"
echo "  Python: rt-stt-cli stream -j -s /tmp/rt-stt-test.sock"
echo ""
echo "To pause/resume:"
echo "  rt-stt-cli pause -s /tmp/rt-stt-test.sock"
echo "  rt-stt-cli resume -s /tmp/rt-stt-test.sock"
