#!/bin/bash

echo "RT-STT Setup Validation"
echo "======================="
echo ""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Check build
echo "Checking build..."
if [ -f "build/rt-stt" ]; then
    echo -e "${GREEN}✓${NC} Binary exists: build/rt-stt"
else
    echo -e "${RED}✗${NC} Binary missing: build/rt-stt"
    echo "  Run: mkdir build && cd build && cmake .. && make -j8"
fi

# Check models
echo ""
echo "Checking models..."
for model in "ggml-base.en.bin" "ggml-small.en.bin"; do
    if [ -f "models/$model" ]; then
        echo -e "${GREEN}✓${NC} Model exists: models/$model"
    else
        echo -e "${YELLOW}⚠${NC} Model missing: models/$model"
    fi
done

# Check configs
echo ""
echo "Checking configs..."
if [ -f "config/local_test_config.json" ]; then
    echo -e "${GREEN}✓${NC} Local test config exists"
else
    echo -e "${RED}✗${NC} Local test config missing"
fi

if [ -f "config/default_config.json" ]; then
    echo -e "${GREEN}✓${NC} Default config exists"
else
    echo -e "${YELLOW}⚠${NC} Default config missing"
fi

# Check scripts
echo ""
echo "Checking scripts..."
SCRIPTS=(
    "test_sensitive.sh"
    "test_ipc_verbose.sh"
    "test_python_client.sh"
    "test_config.sh"
    "test_daemon_local.sh"
    "scripts/install_daemon.sh"
    "scripts/uninstall_daemon.sh"
)

for script in "${SCRIPTS[@]}"; do
    if [ -x "$script" ]; then
        echo -e "${GREEN}✓${NC} Script executable: $script"
    elif [ -f "$script" ]; then
        echo -e "${YELLOW}⚠${NC} Script not executable: $script (run chmod +x)"
    else
        echo -e "${RED}✗${NC} Script missing: $script"
    fi
done

# Check Python
echo ""
echo "Checking Python..."
if command -v python3 &> /dev/null; then
    echo -e "${GREEN}✓${NC} Python3 available"
else
    echo -e "${RED}✗${NC} Python3 not found"
fi

# Check socket cleanup
echo ""
echo "Checking sockets..."
if [ -S "/tmp/rt-stt.sock" ]; then
    echo -e "${YELLOW}⚠${NC} Socket exists: /tmp/rt-stt.sock (from previous run)"
fi
if [ -S "/tmp/rt-stt-test.sock" ]; then
    echo -e "${YELLOW}⚠${NC} Socket exists: /tmp/rt-stt-test.sock (from previous test)"
fi

# Summary
echo ""
echo "Summary"
echo "-------"
echo "Before running tests:"
echo "1. Ensure build is complete"
echo "2. Models are downloaded"
echo "3. Scripts are executable"
echo "4. Clean up old sockets: rm -f /tmp/rt-stt*.sock"
echo ""
echo "Test order:"
echo "1. ./test_sensitive.sh         - Basic STT test"
echo "2. ./test_config.sh           - Config loading test"
echo "3. ./test_daemon_local.sh     - Local daemon test"
echo "4. ./scripts/install_daemon.sh - System installation"
