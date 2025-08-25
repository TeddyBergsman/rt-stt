#!/bin/bash

# RT-STT Daemon Uninstallation Script

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "RT-STT Daemon Uninstaller"
echo "========================="
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

echo "Uninstalling for user: $CURRENT_USER"
echo ""

# Stop and unload service
PLIST_FILE="$USER_HOME/Library/LaunchAgents/com.rt-stt.user.plist"
if [ -f "$PLIST_FILE" ]; then
    echo "Stopping service..."
    if launchctl list | grep -q "com.rt-stt.user"; then
        launchctl unload "$PLIST_FILE" 2>/dev/null || true
        echo "Service stopped."
    fi
    
    echo "Removing launchd plist..."
    rm -f "$PLIST_FILE"
fi

# Remove binaries
echo "Removing binaries..."
sudo rm -f /usr/local/bin/rt-stt
sudo rm -f /usr/local/bin/rt-stt-ctl
sudo rm -f /usr/local/bin/rt-stt-client

# Ask about models
echo ""
read -p "Remove models? (y/N) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Removing models..."
    sudo rm -rf /usr/local/share/rt-stt
fi

# Ask about configuration and logs
echo ""
read -p "Remove configuration and logs? (y/N) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Removing configuration..."
    rm -rf "$USER_HOME/Library/Application Support/rt-stt"
    
    echo "Removing logs..."
    rm -f "$USER_HOME/Library/Logs/rt-stt.log"
    rm -f "$USER_HOME/Library/Logs/rt-stt.error.log"
fi

# Clean up socket
rm -f /tmp/rt-stt.sock

echo ""
echo -e "${GREEN}Uninstallation complete!${NC}"
echo ""
echo "RT-STT daemon has been removed from your system."

# Check if any processes are still running
if pgrep -x "rt-stt" > /dev/null; then
    echo ""
    echo -e "${YELLOW}Warning: RT-STT process is still running.${NC}"
    echo "You may need to restart your system or kill the process manually."
fi
