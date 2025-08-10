#!/bin/bash

# ESP32 Upload and Monitor Script
# This script uploads firmware and then runs monitoring
# Supports different build configurations

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

show_usage() {
    echo "Usage: $0 [BUILD_TYPE]"
    echo ""
    echo "BUILD_TYPE options:"
    echo "  mqtt        MQTT-only build (default, smallest flash usage)"
    echo "  webserver   WebServer-only build"
    echo "  both        Both MQTT and WebServer (full features)"
    echo "  serial      Use serial upload instead of OTA"
    echo ""
    echo "Examples:"
    echo "  $0              # MQTT-only build (default)"
    echo "  $0 mqtt         # MQTT-only build"
    echo "  $0 webserver    # WebServer-only build"
    echo "  $0 both         # Full feature build"
    echo "  $0 serial       # MQTT-only with serial upload"
    echo ""
}

# Parse command line arguments
BUILD_TYPE="${1:-mqtt}"
ENVIRONMENT=""

case "$BUILD_TYPE" in
    "mqtt"|"")
        ENVIRONMENT="esp32-c3-devkitm-1"
        print_banner "ESP32 Upload and Monitor - MQTT Only"
        ;;
    "webserver")
        ENVIRONMENT="esp32-c3-devkitm-1-webserver"
        print_banner "ESP32 Upload and Monitor - WebServer Only"
        ;;
    "both")
        ENVIRONMENT="esp32-c3-devkitm-1-both"
        print_banner "ESP32 Upload and Monitor - Full Features"
        ;;
    "serial")
        ENVIRONMENT="esp32-c3-devkitm-1-serial"
        print_banner "ESP32 Upload and Monitor - MQTT Only (Serial)"
        ;;
    "help"|"-h"|"--help")
        show_usage
        exit 0
        ;;
    *)
        print_status $RED "Error: Unknown build type '$BUILD_TYPE'"
        show_usage
        exit 1
        ;;
esac

# Check if we're in the right directory
if [ ! -f "platformio.ini" ]; then
    print_status $RED "Error: platformio.ini not found. Run this script from the project root."
    exit 1
fi

print_status $CYAN "Building with environment: $ENVIRONMENT"

# Check if device is awake before attempting OTA
print_status $YELLOW "Checking device awake state..."
if ! python3 scripts/check_sleep.py; then
    print_status $RED "Device appears to be sleeping. Aborting upload."
    exit 1
fi

# Step 1: Build the project
print_status $YELLOW "Building project..."
if ~/.local/bin/platformio run --environment $ENVIRONMENT; then
    print_status $GREEN "✓ Build successful"
else
    print_status $RED "✗ Build failed"
    exit 1
fi

# Step 2: Upload firmware
print_status $YELLOW "Uploading firmware..."
if ~/.local/bin/platformio run --environment $ENVIRONMENT --target upload; then
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
echo "  ./upload_and_monitor.sh     # MQTT-only build"
echo "  ./upload_and_monitor.sh both # Full feature build"
echo ""
echo -e "${BLUE}Build configurations:${NC}"
echo "  mqtt        MQTT-only (default, smallest)"
echo "  webserver   WebServer-only"
echo "  both        Full features"
