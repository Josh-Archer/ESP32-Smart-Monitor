#!/bin/bash

# ESP32 HTTP OTA Upload Script
# Builds firmware and uploads it via the signed-HTTP OTA endpoint
# (http://<host>:8267/update) instead of espota.
#
# Why HTTP and not `pio run -t upload` (espota)? espota needs the device
# to open a TCP connection BACK to this machine, which fails from
# sandboxed / NATed environments ("No response from device" after
# "Authenticating...OK"). The HTTP path is a plain outbound POST, so it
# works from anywhere that can reach the device.
#
# The 8267 handler always frames uploads as [firmware][64-byte trailer],
# so scripts/package_ota.py frames the image first (real signature when
# keys/ota_private.pem exists, zero trailer for password-only devices).
# Never POST a raw firmware.bin — the device would flash it truncated
# and fail with "Update.end failed".

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
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

show_usage() {
    echo "Usage: $0 [BUILD_TYPE]"
    echo ""
    echo "BUILD_TYPE options:"
    echo "  mqtt        MQTT-only build (default, smallest flash usage)"
    echo "  webserver   WebServer-only build"
    echo "  both        Both MQTT and WebServer (full features)"
    echo ""
    echo "Environment overrides:"
    echo "  OTA_HOST      Device hostname/IP (default: poop-monitor.local)"
    echo "  OTA_PORT      Signed-HTTP OTA port (default: 8267)"
    echo "  OTA_PASSWORD  OTA password (default: --auth flag in platformio.ini)"
    echo ""
    echo "Examples:"
    echo "  $0              # MQTT-only build, HTTP OTA upload"
    echo "  $0 both         # Full feature build, HTTP OTA upload"
    echo "  OTA_HOST=192.168.1.50 $0 mqtt"
    echo ""
}

# Parse command line arguments
BUILD_TYPE="${1:-mqtt}"
ENVIRONMENT=""

case "$BUILD_TYPE" in
    "mqtt"|"")
        ENVIRONMENT="esp32-c3-devkitm-1"
        print_banner "ESP32 HTTP OTA Upload - MQTT Only"
        ;;
    "webserver")
        ENVIRONMENT="esp32-c3-devkitm-1-webserver"
        print_banner "ESP32 HTTP OTA Upload - WebServer Only"
        ;;
    "both")
        ENVIRONMENT="esp32-c3-devkitm-1-both"
        print_banner "ESP32 HTTP OTA Upload - Full Features"
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

# Locate PlatformIO: project venv first, then the legacy user install, then PATH.
if [ -x "./.venv/bin/pio" ]; then
    PIO="./.venv/bin/pio"
elif [ -x "$HOME/.local/bin/platformio" ]; then
    PIO="$HOME/.local/bin/platformio"
else
    PIO="$(command -v pio || true)"
fi
if [ -z "$PIO" ]; then
    print_status $RED "Error: PlatformIO not found. Create the project env first:"
    print_status $RED "  python3 -m venv .venv && ./.venv/bin/pip install platformio"
    exit 1
fi

OTA_HOST="${OTA_HOST:-poop-monitor.local}"
OTA_PORT="${OTA_PORT:-8267}"
if [ -z "${OTA_PASSWORD:-}" ]; then
    AUTH_REF="$(grep -Eo -- '--auth=[^ ]+' platformio.ini | head -1 | cut -d= -f2)"
    # platformio.ini keeps the secret out of git via ${sysenv.VAR} — resolve it.
    case "$AUTH_REF" in
        '${sysenv.'*'}')
            SYSENV_VAR="${AUTH_REF#\$\{sysenv.}"
            SYSENV_VAR="${SYSENV_VAR%\}}"
            # Indirect expansion only; anything outside [A-Za-z0-9_] resolves empty.
            case "$SYSENV_VAR" in
                ''|*[!A-Za-z0-9_]* ) OTA_PASSWORD="" ;;
                *) OTA_PASSWORD="${!SYSENV_VAR:-}" ;;
            esac
            ;;
        *)
            OTA_PASSWORD="$AUTH_REF"
            ;;
    esac
fi
if [ -z "$OTA_PASSWORD" ]; then
    print_status $RED "Error: OTA password not found. Export OTA_PASSWORD (never commit it)."
    exit 1
fi

FIRMWARE=".pio/build/$ENVIRONMENT/firmware.bin"
PACKAGE=".pio/build/$ENVIRONMENT/firmware.ota.bin"

# Step 0: record the running version for the before/after check
print_status $CYAN "Current device version:"
OLD_VERSION="$(curl -s -m 10 "telnet://$OTA_HOST:23" 2>/dev/null | tr -d '\r' | grep -Eo 'Version: [^ |]+' | head -1 | awk '{print $2}')"
if [ -z "$OLD_VERSION" ]; then
    OLD_VERSION="$(curl -s -m 10 "http://$OTA_HOST/status" 2>/dev/null | grep -Eo '"version":"[^"]+"' | head -1 | cut -d'"' -f4)"
fi
if [ -n "$OLD_VERSION" ]; then
    print_status $CYAN "  $OLD_VERSION"
else
    print_status $YELLOW "  unknown (device not answering telnet/HTTP status — continuing anyway)"
fi

# Step 1: Build the project
print_status $YELLOW "Building project with environment: $ENVIRONMENT..."
if $PIO run --environment "$ENVIRONMENT"; then
    print_status $GREEN "✓ Build successful"
else
    print_status $RED "✗ Build failed"
    exit 1
fi

if [ ! -f "$FIRMWARE" ]; then
    print_status $RED "Error: expected firmware not found at $FIRMWARE"
    exit 1
fi

# Step 2: Frame the image for the 8267 upload path (never POST raw firmware.bin)
print_status $YELLOW "Framing image for HTTP OTA upload..."
if [ -f "keys/ota_private.pem" ]; then
    print_status $CYAN "Signing key found — producing a real signed package"
else
    print_status $CYAN "No signing key — producing an unsigned package for password-only devices"
fi
if python3 scripts/package_ota.py "$FIRMWARE" -o "$PACKAGE"; then
    print_status $GREEN "✓ Package ready: $PACKAGE"
else
    print_status $RED "✗ Packaging failed"
    exit 1
fi

# Step 3: Upload via HTTP POST
print_status $YELLOW "Uploading to http://$OTA_HOST:$OTA_PORT/update ..."
HTTP_BODY="$(mktemp)"
HTTP_CODE="$(curl -s -m 240 -X POST "http://$OTA_HOST:$OTA_PORT/update" \
    -H "X-OTA-Password: $OTA_PASSWORD" \
    -H "Content-Type: application/octet-stream" \
    --data-binary "@$PACKAGE" \
    -o "$HTTP_BODY" -w '%{http_code}')"
DEVICE_MSG="$(head -c 200 "$HTTP_BODY")"
rm -f "$HTTP_BODY"

case "$HTTP_CODE" in
    200)
        print_status $GREEN "✓ Upload accepted: $DEVICE_MSG"
        ;;
    401)
        print_status $RED "✗ Upload rejected (401 Unauthorized): wrong OTA_PASSWORD for $OTA_HOST"
        exit 1
        ;;
    403)
        print_status $RED "✗ Upload rejected (403): device enforces signatures — provision keys and sign the image"
        exit 1
        ;;
    *)
        print_status $RED "✗ Upload failed (HTTP $HTTP_CODE): $DEVICE_MSG"
        exit 1
        ;;
esac

# Step 4: Wait for reboot and verify the new version
print_status $YELLOW "Waiting for device to reboot..."
sleep 20
for i in $(seq 1 10); do
    NEW_VERSION="$(curl -s -m 10 "telnet://$OTA_HOST:23" 2>/dev/null | tr -d '\r' | grep -Eo 'Version: [^ |]+' | head -1 | awk '{print $2}')"
    if [ -z "$NEW_VERSION" ]; then
        NEW_VERSION="$(curl -s -m 10 "http://$OTA_HOST/status" 2>/dev/null | grep -Eo '"version":"[^"]+"' | head -1 | cut -d'"' -f4)"
    fi
    if [ -n "$NEW_VERSION" ]; then
        break
    fi
    print_status $YELLOW "  device not answering yet (attempt $i/10)..."
    sleep 10
done

if [ -z "$NEW_VERSION" ]; then
    print_status $RED "✗ Device did not come back online after reboot — check power/network"
    exit 1
fi

print_banner "HTTP OTA Upload Complete!"
echo -e "${GREEN}Firmware version: ${OLD_VERSION:-unknown} → ${NEW_VERSION}${NC}"
if [ -n "$OLD_VERSION" ] && [ "$OLD_VERSION" = "$NEW_VERSION" ]; then
    echo -e "${YELLOW}Note: version unchanged — the device may have rolled back, or the build had the same version.${NC}"
fi
