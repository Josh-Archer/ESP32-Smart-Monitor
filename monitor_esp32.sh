#!/bin/bash

# ESP32 Monitoring Script
# Usage: ./monitor_esp32.sh [options]

DEVICE_NAME="poop-monitor"
DEVICE_HOST="poop-monitor.local"
DEVICE_IP="192.168.68.61"
TELNET_PORT="23"
WEB_PORT="80"

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

# Function to extract and display device metrics from JSON status
display_device_metrics() {
    local status_response="$1"
    
    if command -v python3 > /dev/null && echo "$status_response" | python3 -m json.tool > /dev/null 2>&1; then
        echo "$status_response" | python3 -m json.tool
        
        # Extract and display key metrics
        echo -e "\n${BLUE}=== Key Metrics ===${NC}"
        
        # DNS Information
        CURRENT_DNS1=$(echo "$status_response" | python3 -c "import json,sys; data=json.load(sys.stdin); print(data.get('current_dns1', 'N/A'))" 2>/dev/null)
        CURRENT_DNS2=$(echo "$status_response" | python3 -c "import json,sys; data=json.load(sys.stdin); print(data.get('current_dns2', 'N/A'))" 2>/dev/null)
        PRIMARY_DNS=$(echo "$status_response" | python3 -c "import json,sys; data=json.load(sys.stdin); print(data.get('primary_dns', 'N/A'))" 2>/dev/null)
        
        echo -e "${YELLOW}DNS Servers:${NC}"
        echo "  Current Primary: $CURRENT_DNS1"
        echo "  Current Secondary: $CURRENT_DNS2" 
        echo "  Configured Primary: $PRIMARY_DNS"
        
        # Heartbeat Information
        LAST_SUCCESS=$(echo "$status_response" | python3 -c "import json,sys; data=json.load(sys.stdin); print(data.get('last_heartbeat_success', 0))" 2>/dev/null)
        LAST_CODE=$(echo "$status_response" | python3 -c "import json,sys; data=json.load(sys.stdin); print(data.get('last_heartbeat_code', 'N/A'))" 2>/dev/null)
        TIME_SINCE_SECONDS=$(echo "$status_response" | python3 -c "import json,sys; data=json.load(sys.stdin); print(data.get('time_since_last_success_seconds', 0))" 2>/dev/null)
        LAST_HEARTBEAT_FORMATTED=$(echo "$status_response" | python3 -c "import json,sys; data=json.load(sys.stdin); print(data.get('last_heartbeat_uptime_formatted', 'Never'))" 2>/dev/null)
        CURRENT_UPTIME_FORMATTED=$(echo "$status_response" | python3 -c "import json,sys; data=json.load(sys.stdin); print(data.get('current_uptime_formatted', 'N/A'))" 2>/dev/null)
        
        echo -e "\n${YELLOW}Heartbeat Status:${NC}"
        if [ "$LAST_SUCCESS" != "0" ]; then
            echo "  Last Success: at uptime $LAST_HEARTBEAT_FORMATTED"
            echo "  Time Since: ${TIME_SINCE_SECONDS}s ago"
        else
            echo "  Last Success: Never (since boot)"
            echo "  Time Since: N/A"
        fi
        echo "  Last Response Code: $LAST_CODE"
        
        # Uptime
        echo -e "\n${YELLOW}Uptime:${NC} $CURRENT_UPTIME_FORMATTED"
        
    else
        echo "$status_response"
    fi
}

print_status $BLUE "=== ESP32 ($DEVICE_NAME) Monitoring Script ==="

# Check if device is reachable via ping
print_status $YELLOW "Checking network connectivity..."
if ping -c 3 -W 3000 $DEVICE_HOST > /dev/null 2>&1; then
    PING_IP=$(ping -c 1 $DEVICE_HOST | grep "PING" | sed 's/.*(\([^)]*\)).*/\1/')
    print_status $GREEN "✓ Device is reachable at $PING_IP"
else
    print_status $RED "✗ Device is not reachable via ping"
    exit 1
fi

# Test telnet connection
print_status $YELLOW "Testing telnet connection (port $TELNET_PORT)..."
if nc -z -w 3 $DEVICE_HOST $TELNET_PORT 2>/dev/null; then
    print_status $GREEN "✓ Telnet server is running"
else
    print_status $RED "✗ Telnet server is not responding"
fi

# Test web server connection
print_status $YELLOW "Testing web server (port $WEB_PORT)..."
if nc -z -w 3 $DEVICE_HOST $WEB_PORT 2>/dev/null; then
    print_status $GREEN "✓ Web server is running"
    
    # Try to get status from web interface
    print_status $YELLOW "Fetching device status..."
    STATUS_RESPONSE=$(curl -s -m 5 http://$DEVICE_HOST/status 2>/dev/null)
    
    if [ $? -eq 0 ] && [ ! -z "$STATUS_RESPONSE" ]; then
        print_status $GREEN "✓ Status endpoint is working"
        echo -e "${BLUE}Device Status:${NC}"
        
        # Display device metrics using shared function
        display_device_metrics "$STATUS_RESPONSE"
        
    else
        print_status $YELLOW "⚠ Status endpoint not responding, trying direct IP..."
        STATUS_RESPONSE=$(curl -s -m 5 http://$DEVICE_IP/status 2>/dev/null)
        if [ $? -eq 0 ] && [ ! -z "$STATUS_RESPONSE" ]; then
            print_status $GREEN "✓ Status endpoint working via IP"
            echo -e "${BLUE}Device Status:${NC}"
            
            # Display device metrics using shared function
            display_device_metrics "$STATUS_RESPONSE"
        else
            print_status $RED "✗ Status endpoint not responding"
        fi
    fi
else
    print_status $RED "✗ Web server is not responding"
fi

# Provide connection commands
echo ""
print_status $BLUE "=== Connection Commands ==="
echo -e "${YELLOW}Telnet:${NC} nc $DEVICE_HOST $TELNET_PORT"
echo -e "${YELLOW}Web Interface:${NC} http://$DEVICE_HOST or http://$DEVICE_IP"
echo -e "${YELLOW}Status JSON:${NC} curl http://$DEVICE_HOST/status"
echo -e "${YELLOW}Reboot Device:${NC} curl http://$DEVICE_HOST/reboot"

# Option to connect to telnet directly
if [ "$1" = "--telnet" ] || [ "$1" = "-t" ]; then
    print_status $YELLOW "Connecting to telnet..."
    nc $DEVICE_HOST $TELNET_PORT
fi

# Option to open web interface
if [ "$1" = "--web" ] || [ "$1" = "-w" ]; then
    print_status $YELLOW "Opening web interface..."
    if command -v open > /dev/null; then
        open http://$DEVICE_HOST
    else
        echo "Web interface: http://$DEVICE_HOST"
    fi
fi
