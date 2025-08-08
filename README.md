
# ESP32 Poop Monitor

An ESP32-based monitoring device with a **modern web interface** (served via Docker/Kubernetes), live console streaming, smart DNS monitoring, and comprehensive automation tools.

## What's New (v2.4+)

- Web UI is now fully decoupled from firmware; all static file serving (SPIFFS) removed from device
- Frontend always communicates with the ESP32 device via mDNS (`http://poop-monitor.local`)
- Added cache-bust button to the web UI for instant updates
- Improved nginx config for SPA fallback and cache control
- Automated Docker build and Kubernetes deployment scripts (`deploy_web.ps1`, `deploy_web.sh`)
- Deployment uses `latest` image tag with `imagePullPolicy: Always` for reliable updates
- Enhanced DNS monitoring, telnet, and OTA workflow scripts
- General code cleanup and documentation improvements

## Features

- 📡 WiFi connectivity with custom DNS configuration and fallback
- 🔄 OTA (Over-The-Air) firmware updates with automatic monitoring
- 🖥️ **Live telnet console streaming** to web interface with syntax highlighting
- 🌐 **Modern web interface** (served via Docker/Kubernetes/nginx) with responsive design and real-time updates
- 🔍 **Intelligent DNS monitoring** with primary/fallback server testing and duplicate detection
- 📊 **Enhanced status monitoring** with formatted timestamps and WiFi signal strength
- 🔧 **Comprehensive automation scripts** for deployment and management (PowerShell & Bash)
- ⏱️ **Smart post-upload monitoring** with automatic device detection
- �️ **Cache-bust button** for instant web UI updates
- 🚀 **One-command Docker/K8s deploy** with reliable image update
- 🔒 **Smart DNS alerting** with configurable timing (5min delay, 30min intervals)

## Security Setup

**IMPORTANT**: This project uses a secure credentials system to protect sensitive information.

### First Time Setup

1. Copy the credentials template:

   `cp src/credentials.template.cpp src/credentials.cpp`

2. Edit `src/credentials.cpp` with your actual values:

   - WiFi SSID and password
   - OTA update password
   - Pushover app token and user key

3. The `credentials.cpp` file is automatically ignored by git, so your secrets won't be committed.

### File Structure

src/
├── main.cpp              # Main program
├── config.h/.cpp         # Non-sensitive configuration
├── credentials.h         # Credentials interface (safe to commit)
├── credentials.cpp       # YOUR SECRETS (never committed)
├── credentials.template.cpp # Template for others (safe to commit)
├── telnet.h/.cpp         # Telnet server
├── notifications.h/.cpp  # Pushover alerts
├── dns_manager.h/.cpp    # DNS testing
├── ota_manager.h/.cpp    # OTA updates
├── system_utils.h/.cpp   # System utilities (reboot, etc.)
└── web_server.h/.cpp     # Web API

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
4. **Manual Override**: Web interface allows pausing alerts temporarily or indefinitely
5. **Auto-Resume**: Alerts automatically resume when DNS recovers or can be manually resumed

## Development Scripts

### PowerShell Scripts (Windows)

The project includes PowerShell scripts for Windows development workflow.

#### Setup

Load the scripts in your PowerShell session:

```powershell
. .\scripts\scripts.ps1
```

Or add to your PowerShell profile for auto-loading:

```powershell
# Add to $PROFILE (run `notepad $PROFILE`)
if (Test-Path "C:\Code\Arduino\scripts\scripts.ps1") {
    . "C:\Code\Arduino\scripts\scripts.ps1"
}
```

#### Available PowerShell Commands

**Build & Deploy:**

- `pio-build` - Build firmware
- `pio-upload-ota` - Upload via OTA to IP address
- `pio-upload-ota-hostname` - Upload via OTA to hostname
- `deploy-ota` - Complete workflow: build + upload + monitor
- `deploy-ota -clean` - Clean build + upload + monitor

**Monitoring:**

- `telnet-monitor` - Connect to ESP32 telnet server
- `pio-monitor` - Serial monitor via PlatformIO

**Network:**

- `ping-device` - Test connectivity to ESP32
- `device-info` - Show device configuration

**Git Workflow:**

- `commit-version "2.1.0" "Smart DNS monitoring and code improvements"` - Commit with version tag
- `git-status-clean` - Git status excluding build files

**Quick Start:**

```powershell
# See all available commands
pio-help

# Build and deploy with monitoring
deploy-ota

# Just upload new firmware
pio-upload-ota

# Monitor telnet output
telnet-monitor
```

### Bash Scripts (Linux/macOS)

#### `monitor_esp32.sh` - Comprehensive Device Monitoring

```bash
./monitor_esp32.sh          # Full diagnostic check
./monitor_esp32.sh --telnet # Connect directly to telnet
./monitor_esp32.sh --web    # Open web interface in browser
```

**Features:**

- Network connectivity check with ping verification
- Service availability testing (telnet, web server)
- **Enhanced DNS monitoring** (primary vs fallback server usage)
- **Formatted heartbeat tracking** (success timestamps, response codes, time-since-last-success)
- **Human-readable uptime displays** (hours, minutes, seconds)
- **Smart JSON parsing** with comprehensive device metrics
- Colored output with timestamps for better readability
- Connection command suggestions for easy access

#### `reboot_esp32.sh` - Remote Device Reboot

```bash
./reboot_esp32.sh           # Interactive reboot with confirmation
./reboot_esp32.sh --force   # Force reboot without confirmation
```

**Features:**

- Safety confirmation dialog (bypass with --force)
- Automatic post-reboot monitoring and verification  
- Smart offline/online detection with retry logic
- Service availability verification after reboot
- **Enhanced status reporting** with uptime and version information
- Comprehensive error handling and user feedback

#### `upload_and_monitor.sh` - All-in-One Deployment

```bash
./upload_and_monitor.sh     # Build, upload, and monitor in one command
```

**Features:**

- Complete PlatformIO build and upload process
- **Smart device initialization waiting** (automatic ping detection)
- **Automatic post-upload monitoring** with comprehensive status check
- Status verification and next-step command suggestions  
- Error handling with clear feedback and troubleshooting steps

## Building and Deploying

### Manual Commands

1. **Build**: `platformio run` or `pio run`
2. **Upload via USB**: `platformio run --target upload` or `pio run --target upload`
3. **Upload via OTA**: `platformio run --target upload` or `pio run --target upload` (after initial USB upload)

### Automated Upload + Monitoring

The project includes **smart automated monitoring** after uploads with enhanced device detection:

**Automatic (Recommended):**

```bash
platformio run --target upload
# or  
pio run --target upload
# Automatically triggers monitor_esp32.sh after successful upload
```

**All-in-One Deployment:**

```bash
./upload_and_monitor.sh
# Builds, uploads, and comprehensively monitors in one command
```

**Enhanced automation features:**

- ⏱️ **Smart device detection** - Intelligently waits for device initialization
- 🔍 **Automatic status verification** - Comprehensive post-upload diagnostics  
- 📊 **Enhanced metrics display** - Formatted timestamps, DNS status, heartbeat tracking
- 🎯 **Actionable feedback** - Clear next steps and troubleshooting guidance
- 🛡️ **Error resilience** - Handles network issues and provides fallback options

## Remote Access

### Web Interface

The ESP32 provides a modern Bootstrap-based web interface accessible at:

- `http://poop-monitor.local/` - Main control panel with alert controls
- `http://poop-monitor.local/status` - JSON status API
- `http://poop-monitor.local/reboot` - Remote reboot

**Modern Interface Features (v2.3.0):**

- **Bootstrap 5.3.0 framework** with professional responsive design
- **Real-time status cards** - version, IP, uptime, WiFi signal strength
- **Interactive dashboard** with auto-refresh every 30 seconds
- **Toast notifications** for user feedback and confirmations
- **Live telnet console** with syntax highlighting and auto-scroll
- **Mobile-responsive** design for smartphone access
- **Connection status indicator** with real-time monitoring

**Live Console Features:**

- **Toggle console window** with real-time telnet log streaming
- **Syntax highlighting** for log levels (error, warning, info, success)
- **Auto-scroll toggle** with manual scroll control
- **Clear console** functionality
- **Terminal styling** with dark theme and scrollbars
- **Keyboard shortcuts** (Ctrl+R refresh, Ctrl+` toggle console)

**Alert Control Panel:**

- **Visual alert status** with color-coded indicators
- **One-click pause controls** - 30min, 1hr, 3hr, or indefinite
- **Smart resume functionality** with confirmation dialogs
- **Time remaining display** for paused alerts
- **Auto-resume on DNS recovery**

**Technical Implementation:**

- **SPIFFS filesystem** serving static files (HTML/CSS/JS)
- **Bootstrap Icons** for professional iconography
- **JavaScript fetch API** for dynamic content loading
- **JSON REST endpoints** for all device interactions
- **Memory optimized** - 23.7% flash reduction vs embedded HTML

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
- **DNS configuration monitoring** (primary, fallback, currently active servers)
- **Alert control status** (paused state, time remaining for temporary pauses)
- **Detailed heartbeat tracking** (last success with formatted uptime, response codes, time-since-last-success in seconds)
- **Formatted timestamps** (human-readable uptime displays)
- System resources (memory usage, connection status)
- **Network diagnostics** with comprehensive status reporting

## Quick Start Guide

1. **Setup credentials**: `cp src/credentials.template.cpp src/credentials.cpp`
2. **Edit credentials**: Add your WiFi, OTA, and Pushover details
3. **Build and upload firmware**: `pio-upload-ota` (or `./upload_and_monitor.sh`)
4. **Upload web interface**: `pio-upload-spiffs` (deploys Bootstrap files)
5. **Access web interface**: `http://poop-monitor.local/`

### Development Workflow

- **Firmware deployment**: `pio-upload-ota` (builds and uploads firmware via OTA)
- **Web interface deployment**: `pio-upload-spiffs` (uploads HTML/CSS/JS files)
- **Combined deployment**: `./upload_and_monitor.sh` (builds, uploads, monitors)
- **Status monitoring**: `./monitor_esp32.sh` (comprehensive diagnostics)
- **Remote management**: `./reboot_esp32.sh` (safe remote reboot)
- **Direct access**: Use `--telnet` or `--web` flags for quick connections

### File Structure for Web Interface

```text
data/
├── index.html      # Bootstrap main interface
├── style.css       # Custom CSS for console & enhancements  
└── app.js          # JavaScript application with live features
```

### PlatformIO Shortcuts

- **Build**: `pio run` (alias for platformio run)
- **Upload**: `pio run --target upload` (with automatic monitoring)
- **Clean build**: `pio run --target clean`

## Troubleshooting

### Common Issues

**Device connectivity issues:**

```bash
./monitor_esp32.sh  # Comprehensive diagnostics with DNS and heartbeat analysis
./reboot_esp32.sh   # Safe remote reboot with post-reboot verification
```

**DNS and network troubleshooting:**

- Monitor shows **current vs configured DNS servers**
- Primary DNS: 192.168.68.51 (local router)
- Fallback: 1.1.1.1 (Cloudflare)
- **Enhanced DNS monitoring** tracks which server is currently in use

**Heartbeat and service monitoring:**

- **Formatted timestamp tracking** shows last successful heartbeat with uptime
- **Time-since-last-success** displayed in human-readable seconds
- Monitor notifications.archerfamily.io accessibility via telnet logs
- **JSON status endpoint** provides comprehensive diagnostic information

**Build and deployment issues:**

- Use `./upload_and_monitor.sh` for **complete build-to-monitoring workflow**
- **Smart device detection** automatically waits for initialization
- **Post-upload verification** ensures successful deployment

## Security Notes

- Never commit `credentials.cpp` - it's in `.gitignore`
- Use strong passwords for OTA updates
- Consider using WPA3 for WiFi if available
- Pushover tokens should be kept secret
