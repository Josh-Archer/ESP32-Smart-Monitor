#!/bin/bash

# ESP32 Reboot Script
# Usage: ./reboot_esp32.sh [options]
#
# Reboots the device from anywhere that can reach the MQTT broker or the
# device itself. Two ways in:
#
#   mqtt  Publish to the firmware's reboot command topic. Works on every
#         build (the MQTT-only build has no web server) and needs no route to
#         the device, only to the broker. The script then watches the
#         device's availability topic for "offline" followed by "online".
#   http  GET http://<host>/reboot. Only builds with the web server answer.
#
# The default, "auto", uses MQTT when a broker is known and mosquitto_pub is
# installed, and HTTP otherwise.
#
# Broker settings come from the environment (MQTT_HOST, MQTT_PORT, MQTT_USER,
# MQTT_PASSWORD) or, when unset, from src/config.cpp and src/credentials.*.
# The password is handed to the mosquitto clients in a private options file,
# never on a command line, and is never printed.
#
# Exit codes: 0 rebooted and back, 1 the command could not be sent,
#             2 sent but the device was not seen coming back in time.

set -u

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

DEVICE_NAME="poop-monitor"
DEVICE_HOST="${DEVICE_HOST:-poop-monitor.local}"

# Must match src/mqtt_manager.cpp (test_reboot_script.py checks it).
MQTT_COMMAND_TOPIC="homeassistant/poop_monitor/command"
MQTT_AVAILABILITY_TOPIC="homeassistant/sensor/poop_monitor/availability"

METHOD="auto"
FORCE_REBOOT=false
WAIT_FOR_DEVICE=true
MAX_WAIT=90

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

show_usage() {
    cat <<USAGE
Usage: $0 [options]

  -f, --force          Do not ask for confirmation
  -m, --method METHOD  mqtt, http or auto (default: auto)
      --host HOST      Device hostname or IP for the HTTP method and ping
                       (default: \$DEVICE_HOST or poop-monitor.local)
      --timeout SEC    How long to wait for the device to come back (default: 90)
      --no-wait        Send the command and exit without waiting
  -h, --help           Show this help

Broker settings: MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASSWORD, or the
values in src/config.cpp and src/credentials.cpp / src/credentials.h.
USAGE
}

while [ $# -gt 0 ]; do
    case "$1" in
        -f|--force) FORCE_REBOOT=true; shift ;;
        -m|--method) METHOD="${2:-}"; shift 2 ;;
        --host) DEVICE_HOST="${2:-}"; shift 2 ;;
        --timeout) MAX_WAIT="${2:-}"; shift 2 ;;
        --no-wait) WAIT_FOR_DEVICE=false; shift ;;
        -h|--help) show_usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; show_usage >&2; exit 1 ;;
    esac
done

case "$METHOD" in
    auto|mqtt|http) ;;
    *) echo "--method must be mqtt, http or auto" >&2; exit 1 ;;
esac
case "$MAX_WAIT" in
    ''|*[!0-9]*) echo "--timeout must be a number of seconds" >&2; exit 1 ;;
esac

# First quoted value assigned to NAME in the given files, for both
#   const char* NAME = "value";   and   #define NAME "value"
read_setting() {
    local name=$1
    shift
    local file value
    for file in "$@"; do
        [ -r "$file" ] || continue
        value=$(sed -n -E "s/^[[:space:]]*(const[[:space:]]+char[[:space:]]*\*[[:space:]]*|#define[[:space:]]+)${name}([[:space:]]*=[[:space:]]*|[[:space:]]+)\"([^\"]*)\".*/\3/p" "$file" | head -n 1)
        if [ -n "$value" ]; then
            printf '%s' "$value"
            return 0
        fi
    done
    return 1
}

CONFIG_CPP="$SCRIPT_DIR/src/config.cpp"
CREDENTIAL_FILES=("$SCRIPT_DIR/src/credentials.cpp" "$SCRIPT_DIR/src/credentials.h")

MQTT_HOST="${MQTT_HOST:-$(read_setting mqttServer "$CONFIG_CPP" || true)}"
MQTT_PORT="${MQTT_PORT:-$(sed -n -E 's/^[[:space:]]*const[[:space:]]+int[[:space:]]+mqttPort[[:space:]]*=[[:space:]]*([0-9]+).*/\1/p' "$CONFIG_CPP" 2>/dev/null | head -n 1)}"
MQTT_PORT="${MQTT_PORT:-1883}"
MQTT_USER="${MQTT_USER-$(read_setting MQTT_USER "${CREDENTIAL_FILES[@]}" || true)}"
MQTT_PASSWORD="${MQTT_PASSWORD-$(read_setting MQTT_PASSWORD "${CREDENTIAL_FILES[@]}" || true)}"

print_status $BLUE "=== ESP32 ($DEVICE_NAME) Reboot Script ==="

if [ "$METHOD" = "auto" ]; then
    if [ -n "$MQTT_HOST" ] && command -v mosquitto_pub >/dev/null 2>&1; then
        METHOD="mqtt"
    else
        METHOD="http"
    fi
fi

if [ "$METHOD" = "mqtt" ]; then
    if ! command -v mosquitto_pub >/dev/null 2>&1; then
        print_status $RED "✗ mosquitto_pub is not installed (package mosquitto-clients)"
        exit 1
    fi
    if [ -z "$MQTT_HOST" ]; then
        print_status $RED "✗ No MQTT broker known: set MQTT_HOST or mqttServer in src/config.cpp"
        exit 1
    fi
    print_status $BLUE "Method: MQTT via $MQTT_HOST:$MQTT_PORT"
else
    print_status $BLUE "Method: HTTP via $DEVICE_HOST"
fi

if [ "$FORCE_REBOOT" = true ]; then
    print_status $YELLOW "Force reboot enabled, skipping confirmation"
else
    echo ""
    echo -e "${YELLOW}⚠️  WARNING: This will reboot the ESP32 device${NC}"
    echo -e "${YELLOW}   Device: $DEVICE_NAME${NC}"
    echo ""
    read -p "Are you sure you want to reboot? (y/N): " -n 1 -r
    echo ""

    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_status $BLUE "Reboot cancelled by user"
        exit 0
    fi
fi

WORK_DIR=""
SUB_PID=""
cleanup() {
    if [ -n "$SUB_PID" ]; then
        kill "$SUB_PID" 2>/dev/null
    fi
    if [ -n "$WORK_DIR" ]; then
        rm -rf "$WORK_DIR"
    fi
}
trap cleanup EXIT

# --- MQTT ---------------------------------------------------------------------

# The mosquitto clients read default options from
# $XDG_CONFIG_HOME/mosquitto_pub and mosquitto_sub, one option per line. That
# keeps the password out of the process list.
write_client_options() {
    local file
    umask 077
    for file in "$WORK_DIR/config/mosquitto_pub" "$WORK_DIR/config/mosquitto_sub"; do
        {
            printf -- '-h %s\n' "$MQTT_HOST"
            printf -- '-p %s\n' "$MQTT_PORT"
            if [ -n "$MQTT_USER" ]; then
                printf -- '-u %s\n' "$MQTT_USER"
                printf -- '-P %s\n' "$MQTT_PASSWORD"
            fi
        } > "$file"
    done
}

# Wait until WORD appears in the availability log after line START.
# Prints the line number it was found on.
wait_for_state() {
    local word=$1 start=$2 deadline=$3 line
    while [ "$(date +%s)" -lt "$deadline" ]; do
        line=$(awk -v start="$start" -v word="$word" 'NR > start && $NF == word { print NR; exit }' "$WORK_DIR/availability" 2>/dev/null)
        if [ -n "$line" ]; then
            echo "$line"
            return 0
        fi
        sleep 0.5
    done
    return 1
}

reboot_via_mqtt() {
    WORK_DIR=$(mktemp -d)
    mkdir -p "$WORK_DIR/config"
    write_client_options
    : > "$WORK_DIR/availability"

    local watching=false
    if [ "$WAIT_FOR_DEVICE" = true ] && command -v mosquitto_sub >/dev/null 2>&1; then
        XDG_CONFIG_HOME="$WORK_DIR/config" mosquitto_sub -v -t "$MQTT_AVAILABILITY_TOPIC" \
            -W $((MAX_WAIT + 15)) >> "$WORK_DIR/availability" 2>/dev/null &
        SUB_PID=$!
        watching=true
        # Let the retained state arrive, so it is not mistaken for the reboot.
        sleep 2
        local current
        current=$(awk 'END { print $NF }' "$WORK_DIR/availability")
        print_status $BLUE "Device availability before the reboot: ${current:-unknown}"
    fi
    local seen
    seen=$(wc -l < "$WORK_DIR/availability")

    print_status $YELLOW "Publishing the reboot command..."
    if ! XDG_CONFIG_HOME="$WORK_DIR/config" mosquitto_pub -q 1 \
            -t "$MQTT_COMMAND_TOPIC/reboot" -m "reboot" 2> "$WORK_DIR/publish.err"; then
        print_status $RED "✗ Could not publish to the broker: $(head -n 1 "$WORK_DIR/publish.err")"
        return 1
    fi
    print_status $GREEN "✓ Reboot command published"

    if [ "$WAIT_FOR_DEVICE" != true ]; then
        return 0
    fi
    if [ "$watching" != true ]; then
        print_status $YELLOW "⚠ mosquitto_sub is not installed; not waiting for the device"
        return 0
    fi

    local deadline offline_line
    deadline=$(( $(date +%s) + MAX_WAIT ))
    print_status $YELLOW "Waiting for the device to go offline..."
    if ! offline_line=$(wait_for_state offline "$seen" "$deadline"); then
        print_status $RED "✗ The device did not report going offline within ${MAX_WAIT}s"
        print_status $YELLOW "  It may not be connected to the broker; check power and Wi-Fi."
        return 2
    fi
    print_status $GREEN "✓ Device went offline"
    print_status $YELLOW "Waiting for the device to come back online..."
    if ! wait_for_state online "$offline_line" "$deadline" >/dev/null; then
        print_status $RED "✗ The device did not come back within ${MAX_WAIT}s"
        return 2
    fi
    print_status $GREEN "✓ Device is back online"
    return 0
}

# --- HTTP ---------------------------------------------------------------------

reboot_via_http() {
    print_status $YELLOW "Checking if device is reachable..."
    if ! ping -c 2 -W 3 "$DEVICE_HOST" > /dev/null 2>&1; then
        print_status $RED "✗ Device is not reachable via ping at $DEVICE_HOST"
        return 1
    fi
    print_status $GREEN "✓ Device is reachable"

    print_status $YELLOW "Attempting reboot via http://$DEVICE_HOST/reboot ..."
    if ! curl -s -f -m 10 "http://$DEVICE_HOST/reboot" > /dev/null 2>&1; then
        print_status $RED "✗ The device did not accept the request (builds without the web server cannot; use --method mqtt)"
        return 1
    fi
    print_status $GREEN "✓ Reboot command sent"

    if [ "$WAIT_FOR_DEVICE" != true ]; then
        return 0
    fi

    print_status $YELLOW "Waiting for device to go offline..."
    sleep 5
    local waited=0
    while ping -c 1 -W 1 "$DEVICE_HOST" > /dev/null 2>&1 && [ $waited -lt 10 ]; do
        sleep 1
        waited=$((waited + 1))
    done
    if [ $waited -lt 10 ]; then
        print_status $GREEN "✓ Device went offline"
    else
        print_status $YELLOW "⚠ Device still responding (may be rebooting)"
    fi

    print_status $YELLOW "Waiting for device to come back online..."
    waited=0
    while ! ping -c 1 -W 1 "$DEVICE_HOST" > /dev/null 2>&1 && [ $waited -lt "$MAX_WAIT" ]; do
        sleep 1
        waited=$((waited + 1))
    done
    if [ $waited -ge "$MAX_WAIT" ]; then
        print_status $RED "✗ Device did not come back online within ${MAX_WAIT}s"
        return 2
    fi
    print_status $GREEN "✓ Device is back online after ${waited}s"
    return 0
}

if [ "$METHOD" = "mqtt" ]; then
    reboot_via_mqtt
else
    reboot_via_http
fi
RESULT=$?

echo ""
case $RESULT in
    0) print_status $GREEN "🎉 Reboot completed successfully!" ;;
    1) print_status $RED "Reboot command was not sent" ;;
    *) print_status $RED "Reboot was requested, but the device was not seen coming back"
       print_status $YELLOW "This could mean:"
       echo "  - Device is taking longer than expected to boot"
       echo "  - Device is not connected to the MQTT broker"
       echo "  - Network connectivity issues" ;;
esac

echo ""
print_status $BLUE "Next steps:"
echo "  ./monitor_esp32.sh          # Monitor device logs"
exit $RESULT
