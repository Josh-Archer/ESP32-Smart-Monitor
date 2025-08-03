#!/bin/bash

# ESP32 Reboot Script
# Usage: ./reboot_esp32.sh [options]

DEVICE_NAME="poop-monitor"
DEVICE_HOST="poop-monitor.local"
DEVICE_IP="192.168.68.61"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}[$(date '+%H:%M:%S')] ${message}${NC}"
}

print_status $BLUE "=== ESP32 ($DEVICE_NAME) Reboot Script ==="

# Check if device is reachable first
print_status $YELLOW "Checking if device is reachable..."
if ! ping -c 2 -W 3000 $DEVICE_HOST > /dev/null 2>&1; then
    print_status $RED "✗ Device is not reachable via ping"
    exit 1
fi

print_status $GREEN "✓ Device is reachable"

# Function to attempt reboot
attempt_reboot() {
    local host=$1
    local label=$2
    
    print_status $YELLOW "Attempting reboot via $label..."
    
    REBOOT_RESPONSE=$(curl -s -m 10 "http://$host/reboot" 2>/dev/null)
    
    if [ $? -eq 0 ]; then
        print_status $GREEN "✓ Reboot command sent successfully via $label"
        return 0
    else
        print_status $RED "✗ Failed to send reboot command via $label"
        return 1
    fi
}

# Check for force flag
FORCE_REBOOT=false
if [ "$1" = "--force" ] || [ "$1" = "-f" ]; then
    FORCE_REBOOT=true
    print_status $YELLOW "Force reboot enabled, skipping confirmation"
else
    # Ask for confirmation
    echo ""
    echo -e "${YELLOW}⚠️  WARNING: This will reboot the ESP32 device${NC}"
    echo -e "${YELLOW}   Device: $DEVICE_NAME${NC}"
    echo -e "${YELLOW}   Host: $DEVICE_HOST${NC}"
    echo ""
    read -p "Are you sure you want to reboot? (y/N): " -n 1 -r
    echo ""
    
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_status $BLUE "Reboot cancelled by user"
        exit 0
    fi
fi

# Try reboot via hostname first, then IP
if attempt_reboot "$DEVICE_HOST" "hostname"; then
    SUCCESS=true
elif attempt_reboot "$DEVICE_IP" "IP address"; then
    SUCCESS=true
else
    print_status $RED "✗ Failed to reboot device via both hostname and IP"
    exit 1
fi

# Monitor the reboot process
print_status $BLUE "Monitoring reboot process..."
echo -e "${BLUE}The device should reboot in 3 seconds...${NC}"

# Wait for device to go offline
print_status $YELLOW "Waiting for device to go offline..."
sleep 5

OFFLINE_WAIT=0
while ping -c 1 -W 1000 $DEVICE_HOST > /dev/null 2>&1 && [ $OFFLINE_WAIT -lt 10 ]; do
    echo -n "."
    sleep 1
    OFFLINE_WAIT=$((OFFLINE_WAIT + 1))
done

if [ $OFFLINE_WAIT -lt 10 ]; then
    print_status $GREEN "✓ Device went offline"
else
    print_status $YELLOW "⚠ Device still responding (may be rebooting)"
fi

# Wait for device to come back online
print_status $YELLOW "Waiting for device to come back online..."
ONLINE_WAIT=0
MAX_WAIT=60

while ! ping -c 1 -W 1000 $DEVICE_HOST > /dev/null 2>&1 && [ $ONLINE_WAIT -lt $MAX_WAIT ]; do
    if [ $((ONLINE_WAIT % 5)) -eq 0 ]; then
        echo -n "$(($ONLINE_WAIT + 1))s"
    else
        echo -n "."
    fi
    sleep 1
    ONLINE_WAIT=$((ONLINE_WAIT + 1))
done

echo ""

if [ $ONLINE_WAIT -lt $MAX_WAIT ]; then
    print_status $GREEN "✓ Device is back online after ${ONLINE_WAIT}s"
    
    # Wait a bit more for services to start
    print_status $YELLOW "Waiting for services to initialize..."
    sleep 5
    
    # Test if web server is responding
    if curl -s -m 5 "http://$DEVICE_HOST/status" > /dev/null 2>&1; then
        print_status $GREEN "✓ Web server is responding"
        
        # Get quick status
        print_status $BLUE "Device status:"
        STATUS=$(curl -s -m 5 "http://$DEVICE_HOST/status" 2>/dev/null)
        if [ ! -z "$STATUS" ]; then
            UPTIME=$(echo "$STATUS" | python3 -c "import json,sys; data=json.load(sys.stdin); print(data.get('uptime', 0))" 2>/dev/null)
            VERSION=$(echo "$STATUS" | python3 -c "import json,sys; data=json.load(sys.stdin); print(data.get('version', 'N/A'))" 2>/dev/null)
            UPTIME_SECONDS=$((UPTIME / 1000))
            echo "  Version: $VERSION"
            echo "  Uptime: ${UPTIME_SECONDS}s"
        fi
    else
        print_status $YELLOW "⚠ Device is online but web server not ready yet"
    fi
    
    print_status $GREEN "🎉 Reboot completed successfully!"
    
else
    print_status $RED "✗ Device did not come back online within ${MAX_WAIT}s"
    print_status $YELLOW "You may need to check the device manually"
    exit 1
fi

# Suggest running monitor script
echo ""
print_status $BLUE "💡 To monitor the device, run: ./monitor_esp32.sh"
