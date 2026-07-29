# ESP32 Poop Monitor

An ESP32-based monitoring device with **Home Assistant integration**, modern web interface, live console streaming, smart DNS monitoring, and comprehensive automation tools.

## What's New (v2.9.0)

### 2.6.2 - OTA Rollback Protection

- **🔄 Automatic OTA Rollback** - Firmware automatically rolls back to previous version after 10 consecutive boot failures
- **📊 Boot Failure Tracking** - Device tracks boot attempts and detects repeated startup failures
- **🚨 Rollback Notifications** - Pushover alerts sent when automatic rollback is triggered
- **📝 Enhanced Logging** - Rollback events logged in telnet console with detailed information
- **🛡️ Firmware Validation** - Successful boots mark firmware as valid to prevent unnecessary rollbacks
- **🔧 Smart Recovery** - Preserves device functionality even with problematic firmware updates

### 2.6.1

- **Conditional Compilation System** - Revolutionary build optimization with three configurations for different deployment needs.
- **MQTT-Only Configuration** - Default optimized build with **39KB flash savings** (991KB vs 1030KB) for Home Assistant-only deployments.
- **WebServer-Only Configuration** - Standalone web interface build with **18KB flash savings** for non-Home Assistant setups.
- **Full Feature Configuration** - Complete build with both MQTT and WebServer (equivalent to previous versions).
- **Enhanced Build Scripts** - Comprehensive shell and PowerShell automation with configuration-specific commands.
- **Preprocessor-Based Optimization** - Clean conditional compilation using `#ifdef` directives for minimal overhead.
- **Build Validation Suite** - Automated testing script (`test_build_configs.sh`) to verify all configurations compile successfully.
- **Flexible Library Dependencies** - Smart library inclusion (PubSubClient only when MQTT enabled).
- **Serial Upload Variants** - Development-friendly serial upload options for all three configurations.
- **Memory Optimization Documentation** - Detailed flash usage comparison and configuration guide.

### 2.4.1

- **WiFi Signal Quality Summary** - Human-readable WiFi quality (`wifi_quality`) in Home Assistant.
- **Improved Memory Reporting** - Free memory now shown as both formatted string and percent (`free_memory_percent`).
- **Last Heartbeat MQTT State** - Home Assistant now receives a formatted uptime for the last heartbeat.
- **Enhanced MQTT Metrics** - More detailed and human-friendly metrics for Home Assistant.
- **CI/CD Improvements** - GitHub Actions workflow for build/tag automation; CI copies `credentials.template.h` for dummy secrets.
- **Credentials split** - Secrets in gitignored `credentials_private.h` with compile-time macro checks; non-secrets in `config.cpp`.
- **Bug Fixes** - JSON escape padding, reboot flag storage, and cache-bust emoji.

---

- **Complete Home Assistant Integration** - MQTT auto-discovery with 12+ entities for comprehensive monitoring
- **Real-time Telnet Logs** - View live device logs directly in Home Assistant
- **Remote Device Control** - Reboot device and control alerts from Home Assistant
- **Enhanced Status Monitoring** - IP address, firmware version, alert status tracking
- **Simplified Configuration** - All settings moved to `src/config.cpp` for easier management
- **Web UI Improvements** - Fully decoupled from device firmware, served via Docker/Kubernetes
- **Cache-Bust Button** - Manual refresh for instant web UI updates  
- **Automated Deployment** - One-command Docker build and Kubernetes deployment
- **Enhanced DNS Monitoring** - Improved telnet and OTA workflow scripts

## Features

- 📡 WiFi connectivity with custom DNS configuration and fallback
- 🔄 OTA (Over-The-Air) firmware updates with **automatic rollback protection**
- 🛡️ **Smart Boot Failure Recovery** - Automatic rollback after 10 consecutive boot failures
- 🏠 **Home Assistant Integration** - MQTT auto-discovery with 12+ sensors and controls
- 📊 **Real-time MQTT Publishing** - Device status, WiFi signal, DNS health, uptime tracking
- 📋 **Live Telnet Logs in HA** - View device console output in Home Assistant
- 🎛️ **Remote Device Control** - Reboot and alert management from Home Assistant
- 🖥️ **Live telnet console streaming** to web interface with syntax highlighting
- 🌐 **Modern web interface** (served via Docker/Kubernetes/nginx) with responsive design
- 🔍 **Intelligent DNS monitoring** with primary/fallback server testing and duplicate detection
- 📊 **Enhanced status monitoring** with formatted timestamps and WiFi signal strength
- 🔧 **Comprehensive automation scripts** for deployment and management (PowerShell & Bash)
- ⏱️ **Smart post-upload monitoring** with automatic device detection
- 🧹 **Cache-bust button** for instant web UI updates
- 🚀 **One-command Docker/K8s deploy** with reliable image update
- 🔒 **Smart DNS alerting** with configurable timing (5min delay, 30min intervals)

## Configuration Setup

Secrets and non-secret config are **split on purpose** so you do not flash a device with the wrong (or empty) credential path.

| What | Where | Git |
|------|--------|-----|
| **Secrets** (WiFi, OTA password, Pushover, MQTT auth) | `src/credentials_private.h` | **gitignored** — never commit |
| **Non-secrets** (device name, MQTT host/port, DNS, API URLs) | `src/config.cpp` | committed |

### 1. Create private credentials (required)

```bash
# From the repo root
cp credentials.template.h src/credentials_private.h
# Then edit src/credentials_private.h and replace every YOUR_* value
```

PowerShell:

```powershell
Copy-Item credentials.template.h src/credentials_private.h
# Edit src/credentials_private.h — fill in YOUR_* placeholders
```

`src/credentials.h` enforces the required macros at **compile time** (`#error` if any are missing). CI copies the same template with dummy values so PRs build without real secrets.

Example `src/credentials_private.h`:

```cpp
#pragma once
#define WIFI_SSID       "MyNetwork"
#define WIFI_PASSWORD   "secret"
#define OTA_PASSWORD    "ota-secret"
#define PUSHOVER_TOKEN  "app-token"
#define PUSHOVER_USER   "user-key"
#define MQTT_USER       "mqtt-user"   // or ""
#define MQTT_PASSWORD   "mqtt-pass"   // or ""
```

### 2. Edit non-secret config

Edit `src/config.cpp` for broker host, DNS, device name, etc.:

```cpp
const char* mqttServer = "192.168.1.100";  // MQTT broker IP or hostname
const int mqttPort = 1883;
const char* deviceName = "poop-monitor";
// DNS, API endpoint, etc.
```

Do **not** put WiFi passwords or tokens in `config.cpp` — they belong in `credentials_private.h`.

### File Structure

```
credentials.template.h    # Template → copy to src/credentials_private.h
src/
├── main.cpp
├── credentials.h         # Compile-time checks; includes credentials_private.h
├── credentials_private.h # YOUR secrets (gitignored; not in repo)
├── config.h/.cpp         # Non-secret config + wires credential macros
├── telnet.h/.cpp
├── notifications.h/.cpp
├── dns_manager.h/.cpp
├── ota_manager.h/.cpp
├── system_utils.h/.cpp
├── web_server.h/.cpp
└── mqtt_manager.h/.cpp
```

## Home Assistant Integration

The ESP32 provides comprehensive Home Assistant integration through MQTT auto-discovery.

### MQTT Broker Setup

To get MQTT working with Home Assistant:

1. **Install MQTT Broker** (if not already installed):
   - Go to **Settings** → **Add-ons** → **Add-on Store**
   - Search for "Mosquitto broker" and install it
   - Start the Mosquitto broker add-on

2. **Configure MQTT Integration**:
   - Go to **Settings** → **Devices & Services** → **Add Integration**
   - Search for "MQTT" and add it
   - Use `127.0.0.1` (localhost) as broker if using the add-on
   - Create a username/password or leave empty for no authentication

3. **Update ESP32 Config**:
   - Set `mqttServer` / `mqttPort` in `src/config.cpp` to your Home Assistant broker
   - Set `MQTT_USER` / `MQTT_PASSWORD` in `src/credentials_private.h` if the broker requires auth
   - Use `#define MQTT_USER ""` and `#define MQTT_PASSWORD ""` if no authentication

### Home Assistant Entities

Once configured, these entities automatically appear in Home Assistant:

**Sensors:**
- **Device Status** - Overall status with JSON attributes
- **WiFi Signal Strength** - Real-time signal in dBm and percentage  
- **WiFi Quality** - Human-readable WiFi quality (Excellent/Good/Ok/Poor)
- **DNS Connectivity** - Binary sensor showing DNS working/failed
- **Device Uptime** - Uptime tracking with duration device class
- **Free Memory** - Memory usage monitoring in bytes
- **Free Memory Percent** - Memory usage as a percentage
- **Last Heartbeat** - Timestamp of last successful heartbeat
- **IP Address** - Current device IP address
- **Firmware Version** - Current firmware version
- **Telnet Log** - Live device console output

**Controls:**
- **Alert Control Switch** - Enable/disable notifications remotely
- **Reboot Button** - Safely reboot the device from Home Assistant (stores request in NVS/Preferences)

**Features:**
- **Availability Monitoring** - Home Assistant tracks device online/offline status
- **Real-time Data** - Status published every 30 seconds for live monitoring
- **Historical Graphs** - WiFi signal, uptime, memory usage over time
- **Automation Ready** - Use any sensor for Home Assistant automations

### MQTT Topics

The device uses standard Home Assistant discovery topics:

- **Discovery**: `homeassistant/sensor/poop_monitor/*/config`
- **Status**: `homeassistant/sensor/poop_monitor/status`  
- **Availability**: `homeassistant/sensor/poop_monitor/availability`
- **Telnet Logs**: `homeassistant/sensor/poop_monitor/telnet`
- **Commands**: `homeassistant/poop_monitor/command/*`

### Home Assistant Dashboard Example

Create dashboards with:
- Real-time WiFi signal strength graphs
- DNS connectivity status indicators  
- Device uptime and memory usage charts
- Live telnet log display
- Alert control switches
- Reboot button for remote management
- Automation triggers for device offline alerts

## Smart DNS Monitoring

The ESP32 features intelligent DNS monitoring with configurable alerting to prevent notification spam while ensuring you're informed of important network issues.

### Key Features

**🕐 Smart Timing Logic:**

- **5-minute delay** before first alert (avoids temporary network hiccups)
- **30-minute intervals** for subsequent alerts (prevents notification spam)
- **Immediate recovery notifications** when DNS comes back online

**🔍 Intelligent Server Detection:**

- **Duplicate detection** - skips critical alerts when primary/fallback are the same server
- **Fallback optimization** - skips fallback testing when servers are identical
- **Detailed logging** - tracks failure duration and alert timing

**🔕 Manual Alert Control:**

- **Web-based pause controls** - pause alerts for 30min, 1hr, 3hr, or indefinitely
- **Home Assistant control** - Enable/disable alerts remotely
- **Auto-resume on recovery** - alerts automatically resume when DNS recovers
- **Smart timing preservation** - maintains alert intervals when paused/resumed

**📱 Configurable Alert Levels:**

- **Level 1**: Primary DNS down, fallback working (non-critical)
- **Level 2**: Complete DNS failure with different servers (critical)
- **Level 0**: Recovery notifications (informational)

### How It Works

1. **Primary DNS Test**: Tests connectivity using external service
2. **Smart Fallback Logic**: Only tests fallback if different from primary server
3. **Intelligent Alerting**: Only alerts after 5 minutes down, then every 30 minutes
4. **Manual Override**: Web interface and Home Assistant allow pausing alerts
5. **Auto-Resume**: Alerts automatically resume when DNS recovers

## OTA Rollback Protection

The ESP32 Smart Monitor includes robust automatic rollback protection to ensure device reliability even with problematic firmware updates.

### How Rollback Works

1. **Boot Failure Tracking**: Device tracks consecutive boot failures using ESP32's non-volatile storage
2. **Automatic Triggering**: After 10 consecutive boot failures, rollback is automatically triggered  
3. **ESP32 Native Rollback**: Uses ESP32's built-in OTA rollback functionality to revert to previous firmware
4. **Notification System**: Sends high-priority Pushover alert when rollback occurs
5. **Logging**: All rollback events are logged in telnet console with detailed information

### Rollback Process

**Normal Operation:**
- Device boots successfully → Firmware marked as valid → Boot failure counter reset
- Successful operation prevents unnecessary rollbacks

**Failure Recovery:**
- Boot failure detected → Counter incremented → Logged to console
- After 10 failures → Rollback triggered → Previous firmware restored
- Pushover alert sent: "Device experienced 10+ boot failures. Rolling back from firmware vX.X.X to previous version."

### Technical Details

- **Native ESP32 Support**: Uses `esp_ota_mark_app_valid_cancel_rollback()` and `esp_ota_mark_app_invalid_rollback_and_reboot()`
- **Persistent Storage**: Boot failure count stored in NVS (survives power cycles)
- **Safe Defaults**: Only triggers on consecutive failures (not random crashes)
- **Automatic Recovery**: Rollback clears failure counter for fresh start

### Monitoring Rollbacks

- **Telnet Logs**: Real-time rollback status in console output
- **Pushover Alerts**: High-priority notifications when rollback occurs
- **MQTT Integration**: Rollback events can be monitored via Home Assistant
- **Version Tracking**: Device logs firmware version changes for debugging

## Development Scripts

### PowerShell Scripts (Windows)

The project includes PowerShell scripts for Windows development workflow.

#### Setup

Load the scripts in your PowerShell session:

```powershell
. .\scripts\scripts.ps1
```

#### Available PowerShell Commands

**Build & Deploy:**

- `pio-build` - Build firmware (default: MQTT-only)
- `pio-build-mqtt` - Build MQTT-only (smallest flash usage)
- `pio-build-webserver` - Build WebServer-only  
- `pio-build-both` - Build with both MQTT and WebServer
- `pio-upload-ota` - Upload via OTA to hostname
- `deploy-ota` - Complete workflow: build + upload + monitor (MQTT-only)
- `deploy-ota -Environment esp32-c3-devkitm-1-both` - Deploy full features
- `deploy-ota -clean` - Clean build + upload + monitor

**Monitoring:**

- `telnet-monitor` - Connect to ESP32 telnet server
- `pio-monitor` - Serial monitor via PlatformIO

**Network:**

- `ping-device` - Test connectivity to ESP32
- `device-info` - Show device configuration
- `open-device-ui` - Open device web interface

**Docker/K8s:**

- `web-deploy` - Build, push, and deploy web UI to Kubernetes

**Git Workflow:**

- `commit-version "2.3.3" "Home Assistant integration"` - Commit with version tag
- `git-status-clean` - Git status excluding build files

### Bash Scripts (Linux/macOS)

#### `monitor_esp32.sh` - Comprehensive Device Monitoring

```bash
./monitor_esp32.sh          # Full diagnostic check
./monitor_esp32.sh --telnet # Connect directly to telnet
./monitor_esp32.sh --web    # Open web interface in browser
```

#### `reboot_esp32.sh` - Remote Device Reboot

```bash
./reboot_esp32.sh           # Interactive reboot with confirmation
./reboot_esp32.sh --force   # Force reboot without confirmation
```

#### `upload_and_monitor.sh` - All-in-One Deployment

```bash
./upload_and_monitor.sh          # MQTT-only build (default, smallest)
./upload_and_monitor.sh mqtt     # MQTT-only build  
./upload_and_monitor.sh webserver # WebServer-only build
./upload_and_monitor.sh both     # Full feature build
./upload_and_monitor.sh serial   # MQTT-only with serial upload
```

## Building and Deploying

### Build Configurations

The ESP32 firmware supports three build configurations to optimize flash usage:

#### 1. MQTT-Only (Default) - **Recommended**
- **Flash usage**: ~991KB (75.6%) - **Smallest**
- **Features**: Home Assistant integration, telnet console, OTA updates
- **Use case**: Primary monitoring with Home Assistant
- **Build**: `esp32-c3-devkitm-1` (default environment)

```bash
# Shell
./upload_and_monitor.sh          # MQTT-only (default)
./upload_and_monitor.sh mqtt     # Explicit MQTT-only

# PowerShell  
pio-build-mqtt                   # Build MQTT-only
deploy-ota                       # Deploy MQTT-only
```

#### 2. WebServer-Only
- **Flash usage**: ~1011KB (77.2%)
- **Features**: Web interface, API endpoints, telnet console, OTA updates  
- **Use case**: Standalone monitoring without Home Assistant
- **Build**: `esp32-c3-devkitm-1-webserver`

```bash
# Shell
./upload_and_monitor.sh webserver

# PowerShell
pio-build-webserver
deploy-ota -Environment esp32-c3-devkitm-1-webserver
```

#### 3. Both MQTT and WebServer
- **Flash usage**: ~1030KB (78.6%) - **Largest**
- **Features**: Full feature set (equivalent to previous versions)
- **Use case**: Maximum flexibility with both integration options
- **Build**: `esp32-c3-devkitm-1-both`

```bash
# Shell  
./upload_and_monitor.sh both

# PowerShell
pio-build-both
deploy-ota -Environment esp32-c3-devkitm-1-both
```

### Flash Usage Comparison

| Configuration | Flash Usage | Savings vs Both | Features |
|---------------|-------------|-----------------|----------|
| **MQTT-only** | 991KB (75.6%) | **39KB saved** | HA integration, telnet, OTA |
| WebServer-only | 1011KB (77.2%) | **18KB saved** | Web UI, API, telnet, OTA |
| Both | 1030KB (78.6%) | - | Full feature set |

### Quick Start

1. **Secrets**: `cp credentials.template.h src/credentials_private.h` and fill in WiFi/OTA/Pushover/MQTT auth
2. **Non-secrets**: Edit `src/config.cpp` (MQTT host, DNS, device name, etc.)
3. **Build and upload**: `pio-upload-ota` (or `./upload_and_monitor.sh`)
4. **Access device**:
   - **MQTT-only**: Home Assistant auto-discovery
   - **WebServer/Both**: `http://poop-monitor.local/`

### Development Workflow

- **Firmware deployment**: `pio-upload-ota` (builds and uploads MQTT-only firmware via OTA)
- **Combined deployment**: `./upload_and_monitor.sh [mqtt|webserver|both]` (builds, uploads, monitors)
- **Status monitoring**: `./monitor_esp32.sh` (comprehensive diagnostics)
- **Remote management**: `./reboot_esp32.sh` (safe remote reboot)
- **Web UI deployment**: `web-deploy` (Docker/Kubernetes deployment)

**Default behavior**: All scripts now default to MQTT-only builds for optimal flash usage.

## Remote Access

### Web Interface

The ESP32 provides a modern web interface accessible at:

- `http://poop-monitor.local/` - Main control panel with alert controls
- `http://poop-monitor.local/status` - JSON status API
- `http://poop-monitor.local/reboot` - Remote reboot

### Telnet Console

Connect via telnet to see live logs:

```bash
nc poop-monitor.local 23
# or
telnet poop-monitor.local 23  # if telnet is installed
```

### Status API

Get detailed device information:

```bash
curl http://poop-monitor.local/status | python3 -m json.tool
```

**Enhanced API Response includes:**

- Device identification (name, version, IP, WiFi strength)
- DNS configuration monitoring (primary, fallback, currently active servers)
- Alert control status (paused state, time remaining)
- Detailed heartbeat tracking with formatted timestamps
- System resources (memory usage, connection status)
- MQTT connection status

## Troubleshooting

### Common Issues

**Device connectivity issues:**

```bash
./monitor_esp32.sh  # Comprehensive diagnostics
./reboot_esp32.sh   # Safe remote reboot
```

**Home Assistant MQTT issues:**

- Verify MQTT broker is running in Home Assistant
- Check MQTT integration is configured
- Ensure ESP32 `mqttServer` points to Home Assistant IP
- Check Home Assistant logs for MQTT connection errors

**Build and deployment issues:**

- Use `./upload_and_monitor.sh` for complete build-to-monitoring workflow
- If the compiler reports `Missing WIFI_SSID` (or similar): copy `credentials.template.h` → `src/credentials_private.h` and set every macro
- Non-secret settings (MQTT host, DNS) live in `src/config.cpp`; secrets must not


## Development & CI

- **GitHub Actions Workflow:** Automated build and tag for releases; builds use `credentials.template.h` as dummy secrets (never real credentials).
- **🤖 Smart Version Tagging:** Intelligent semantic versioning based on commit analysis and PR content.
  - Analyzes changes to determine appropriate version increments (major/minor/patch)
  - Automatically updates `src/config.cpp` and `README.md` with new versions
  - Supports manual overrides via issue labels (`major`, `minor`, `patch`)
  - Ignores infrastructure-only changes (CI, Docker, scripts)
  - See [`docs/VERSION_TAGGING.md`](docs/VERSION_TAGGING.md) for complete documentation

### Version Management Commands

```bash
# Preview version changes
./scripts/preview_version.sh

# Apply automatic version increment  
python3 scripts/version_manager.py

# Force specific version
python3 scripts/version_manager.py --force-version 2.5.0
```

**PowerShell (Windows):**
```powershell
preview-version     # Preview changes
update-version      # Apply automatic increment
force-version "2.5.0"  # Force specific version
```

## Security Notes

- Store secrets only in `src/credentials_private.h` (gitignored); never commit real tokens/passwords
- Keep non-secret config in `src/config.cpp`
- Use strong passwords for OTA updates and MQTT authentication
- Consider using WPA3 for WiFi if available
- MQTT credentials should match your Home Assistant MQTT user configuration
