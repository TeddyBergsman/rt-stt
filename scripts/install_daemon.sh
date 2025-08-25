#!/bin/bash

# RT-STT Daemon Installation Script

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "RT-STT Daemon Installer"
echo "======================="
echo ""

# Check if running with sudo
if [ "$EUID" -eq 0 ]; then 
   echo -e "${RED}Do not run this script with sudo!${NC}"
   echo "The script will ask for sudo when needed."
   exit 1
fi

# Get current user
CURRENT_USER=$USER
USER_HOME=$HOME

echo "Installing for user: $CURRENT_USER"
echo "Home directory: $USER_HOME"
echo ""

# Check dependencies
echo "Checking dependencies..."

if ! command -v cmake &> /dev/null; then
    echo -e "${RED}cmake not found. Please install cmake first.${NC}"
    exit 1
fi

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

# Build the project
echo ""
echo "Building RT-STT..."
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
cd ..

# Create directories
echo ""
echo "Creating directories..."
sudo mkdir -p /usr/local/bin
sudo mkdir -p /usr/local/share/rt-stt/models
mkdir -p "$USER_HOME/Library/Application Support/rt-stt"
mkdir -p "$USER_HOME/Library/Logs"

# Copy binary
echo "Installing binary..."
sudo cp build/rt-stt /usr/local/bin/
sudo chmod 755 /usr/local/bin/rt-stt

# Copy models
echo "Copying models..."
if [ -d "models" ]; then
    for model in models/*.bin; do
        if [ -f "$model" ]; then
            echo "  - $(basename "$model")"
            sudo cp "$model" /usr/local/share/rt-stt/models/
        fi
    done
else
    echo -e "${YELLOW}Warning: No models found. Please download models separately.${NC}"
fi

# Create/update configuration
CONFIG_FILE="$USER_HOME/Library/Application Support/rt-stt/config.json"
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Creating default configuration..."
    cp config/default_config.json "$CONFIG_FILE"
    
    # Update paths in config
    sed -i '' "s|/usr/local/share/rt-stt/models/|/usr/local/share/rt-stt/models/|g" "$CONFIG_FILE"
    sed -i '' "s|~/Library/Logs/|$USER_HOME/Library/Logs/|g" "$CONFIG_FILE"
else
    echo "Configuration already exists at: $CONFIG_FILE"
fi

# Install launchd plist
echo ""
echo "Installing launchd service..."
PLIST_FILE="$USER_HOME/Library/LaunchAgents/com.rt-stt.user.plist"
mkdir -p "$USER_HOME/Library/LaunchAgents"

# Copy and customize plist
cp com.rt-stt.user.plist "$PLIST_FILE"
sed -i '' "s|USER_NAME|$CURRENT_USER|g" "$PLIST_FILE"

# Unload if already loaded
if launchctl list | grep -q "com.rt-stt.user"; then
    echo "Unloading existing service..."
    launchctl unload "$PLIST_FILE" 2>/dev/null || true
fi

# Load the service
echo "Loading service..."
launchctl load "$PLIST_FILE"

# Create control script
echo ""
echo "Creating control script..."
sudo tee /usr/local/bin/rt-stt-ctl > /dev/null << 'EOF'
#!/bin/bash

# RT-STT Control Script

case "$1" in
    start)
        launchctl load ~/Library/LaunchAgents/com.rt-stt.user.plist
        echo "RT-STT daemon started"
        ;;
    stop)
        launchctl unload ~/Library/LaunchAgents/com.rt-stt.user.plist
        echo "RT-STT daemon stopped"
        ;;
    restart)
        launchctl unload ~/Library/LaunchAgents/com.rt-stt.user.plist 2>/dev/null || true
        sleep 1
        launchctl load ~/Library/LaunchAgents/com.rt-stt.user.plist
        echo "RT-STT daemon restarted"
        ;;
    status)
        if launchctl list | grep -q "com.rt-stt.user"; then
            echo "RT-STT daemon is running"
            launchctl list | grep "com.rt-stt.user"
        else
            echo "RT-STT daemon is not running"
        fi
        ;;
    logs)
        tail -f ~/Library/Logs/rt-stt.log
        ;;
    errors)
        tail -f ~/Library/Logs/rt-stt.error.log
        ;;
    *)
        echo "Usage: rt-stt-ctl {start|stop|restart|status|logs|errors}"
        exit 1
        ;;
esac
EOF

sudo chmod 755 /usr/local/bin/rt-stt-ctl

# Create Python client symlink
if [ -f "examples/python_client.py" ]; then
    echo "Installing Python client..."
    sudo cp examples/python_client.py /usr/local/bin/rt-stt-client
    sudo chmod 755 /usr/local/bin/rt-stt-client
    
    # Add shebang if not present
    if ! head -1 /usr/local/bin/rt-stt-client | grep -q "^#!/usr/bin/env python3"; then
        sudo sed -i '' '1s/^/#!/usr/bin/env python3\n/' /usr/local/bin/rt-stt-client
    fi
fi

echo ""
echo -e "${GREEN}Installation complete!${NC}"
echo ""
echo "RT-STT daemon has been installed and started."
echo ""
echo "Commands:"
echo "  rt-stt-ctl start    - Start the daemon"
echo "  rt-stt-ctl stop     - Stop the daemon"
echo "  rt-stt-ctl restart  - Restart the daemon"
echo "  rt-stt-ctl status   - Check daemon status"
echo "  rt-stt-ctl logs     - View logs"
echo "  rt-stt-ctl errors   - View error logs"
echo ""
echo "Python client:"
echo "  rt-stt-client       - Connect to the daemon"
echo ""
echo "Configuration file:"
echo "  $CONFIG_FILE"
echo ""
echo "Logs:"
echo "  ~/Library/Logs/rt-stt.log"
echo "  ~/Library/Logs/rt-stt.error.log"
echo ""

# Check if service is running
sleep 2
if launchctl list | grep -q "com.rt-stt.user"; then
    echo -e "${GREEN}✓ Service is running${NC}"
    
    # Test connection
    if nc -z localhost 2>/dev/null || nc -z /tmp/rt-stt.sock 2>/dev/null; then
        echo -e "${GREEN}✓ IPC socket is accessible${NC}"
    else
        echo -e "${YELLOW}⚠ IPC socket not yet accessible, may still be starting...${NC}"
    fi
else
    echo -e "${RED}✗ Service failed to start${NC}"
    echo "Check error log: rt-stt-ctl errors"
fi
