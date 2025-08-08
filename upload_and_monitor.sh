#!/bin/bash

# ESP32 Upload and Monitor Script
# This script uploads firmware and then runs monitoring

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}[$(date '+%H:%M:%S')] ${message}${NC}"
}

print_banner() {
    local message=$1
    echo -e "\n${BLUE}============================================================${NC}"
    echo -e "${BLUE}$(printf '%*s' $(((60+${#message})/2)) "$message")${NC}"
    echo -e "${BLUE}============================================================${NC}"
}

print_banner "ESP32 Upload and Monitor"

# Check if we're in the right directory
if [ ! -f "platformio.ini" ]; then
    print_status $RED "Error: platformio.ini not found. Run this script from the project root."
    exit 1
fi

# Step 1: Build the project
print_status $YELLOW "Building project..."
if ~/.platformio/penv/bin/platformio run; then
    print_status $GREEN "✓ Build successful"
else
    print_status $RED "✗ Build failed"
    exit 1
fi

# Step 2: Upload firmware
print_status $YELLOW "Uploading firmware..."
if ~/.platformio/penv/bin/platformio run --target upload; then
    print_status $GREEN "✓ Upload successful"
else
    print_status $RED "✗ Upload failed"
    exit 1
fi

# Step 3: Wait for device to initialize
print_status $YELLOW "Waiting for device to initialize..."
sleep 8

# Step 4: Run monitoring script
if [ -f "monitor_esp32.sh" ]; then
    print_banner "Running Device Monitor"
    ./monitor_esp32.sh
else
    print_status $YELLOW "Monitor script not found, skipping monitoring"
fi

print_banner "Upload and Monitor Complete!"
echo -e "${GREEN}Your ESP32 has been updated and is being monitored.${NC}"
echo ""
echo -e "${BLUE}Available commands:${NC}"
echo "  ./monitor_esp32.sh          # Run monitor again"
echo "  ./monitor_esp32.sh --telnet # Connect to telnet"
echo "  ./monitor_esp32.sh --web    # Open web interface"
echo "  ./reboot_esp32.sh           # Remote reboot"
echo "  ./upload_and_monitor.sh     # Run this script again"
