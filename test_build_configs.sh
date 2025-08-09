#!/bin/bash

# Test script to validate all build configurations
# This ensures all three environments compile successfully

set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

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

print_banner "ESP32 Build Configuration Tests"

if [ ! -f "platformio.ini" ]; then
    print_status $RED "Error: platformio.ini not found. Run this script from the project root."
    exit 1
fi

# Test 1: MQTT-only build (default)
print_status $YELLOW "Testing MQTT-only build..."
if ~/.local/bin/platformio run --environment esp32-c3-devkitm-1 > /tmp/build-mqtt.log 2>&1; then
    MQTT_SIZE=$(grep "Flash:" /tmp/build-mqtt.log | grep -o '[0-9]* bytes' | head -1)
    print_status $GREEN "✓ MQTT-only build successful - $MQTT_SIZE"
else
    print_status $RED "✗ MQTT-only build failed"
    tail -20 /tmp/build-mqtt.log
    exit 1
fi

# Test 2: WebServer-only build  
print_status $YELLOW "Testing WebServer-only build..."
if ~/.local/bin/platformio run --environment esp32-c3-devkitm-1-webserver > /tmp/build-web.log 2>&1; then
    WEB_SIZE=$(grep "Flash:" /tmp/build-web.log | grep -o '[0-9]* bytes' | head -1)
    print_status $GREEN "✓ WebServer-only build successful - $WEB_SIZE"
else
    print_status $RED "✗ WebServer-only build failed"
    tail -20 /tmp/build-web.log
    exit 1
fi

# Test 3: Both MQTT and WebServer build
print_status $YELLOW "Testing both MQTT and WebServer build..."
if ~/.local/bin/platformio run --environment esp32-c3-devkitm-1-both > /tmp/build-both.log 2>&1; then
    BOTH_SIZE=$(grep "Flash:" /tmp/build-both.log | grep -o '[0-9]* bytes' | head -1)
    print_status $GREEN "✓ Both MQTT and WebServer build successful - $BOTH_SIZE"
else
    print_status $RED "✗ Both MQTT and WebServer build failed"
    tail -20 /tmp/build-both.log
    exit 1
fi

# Test 4: Serial variants
print_status $YELLOW "Testing serial build variants..."
for env in esp32-c3-devkitm-1-serial esp32-c3-devkitm-1-webserver-serial esp32-c3-devkitm-1-both-serial; do
    if ~/.local/bin/platformio run --environment $env > /tmp/build-$env.log 2>&1; then
        print_status $GREEN "✓ $env build successful"
    else
        print_status $RED "✗ $env build failed"
        exit 1
    fi
done

print_banner "All Build Tests Passed!"
echo -e "${GREEN}All build configurations compile successfully.${NC}"
echo ""
echo -e "${BLUE}Flash usage summary:${NC}"
echo -e "  MQTT-only:      $MQTT_SIZE"
echo -e "  WebServer-only: $WEB_SIZE" 
echo -e "  Both:           $BOTH_SIZE"
echo ""
echo -e "${YELLOW}The MQTT-only configuration is now the default for optimal flash usage.${NC}"