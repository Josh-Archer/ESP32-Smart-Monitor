# ESP32 Poop Monitor

An ESP32-based monitoring device that sends heartbeat requests and provides remote telnet access, web interface, and comprehensive monitoring tools with automated deployment and management scripts.

## Features

- 📡 WiFi connectivity with custom DNS configuration and fallback
- 🔄 OTA (Over-The-Air) firmware updates with automatic monitoring
- 📱 **Smart DNS alerting** with configurable timing (5min delay, 30min intervals)
- 🖥️ Telnet server for real-time debugging and monitoring
- 🌐 Web interface with remote reboot capability and JSON API
- 🔍 **Intelligent DNS monitoring** with primary/fallback server testing and duplicate detection
- 💾 Persistent version tracking and configuration
- 📊 **Enhanced status monitoring** with formatted timestamps and uptime
- 🔧 **Comprehensive automation scripts** for deployment and management
- ⏱️ **Smart post-upload monitoring** with automatic device detection
- 🛠️ **Centralized utilities** with improved code organization and readability

## Security Setup

**IMPORTANT**: This project uses a secure credentials system to protect sensitive information.

### First Time Setup

1. Copy the credentials template:

   ```bash
   cp src/credentials.template.cpp src/credentials.cpp
   ```

2. Edit `src/credentials.cpp` with your actual values:
   - WiFi SSID and password
   - OTA update password
   - Pushover app token and user key

3. The `credentials.cpp` file is automatically ignored by git, so your secrets won't be committed.

### File Structure

```
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
└── web_server.h/.cpp     # Web interface and API
```

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

The ESP32 provides a web interface accessible at:

- `http://poop-monitor.local/` - Main control panel with alert controls
- `http://poop-monitor.local/status` - JSON status API
- `http://poop-monitor.local/reboot` - Remote reboot

**Control Panel Features:**

- Real-time device information display
- **Alert status and control** - pause/resume DNS failure notifications
- **Flexible pause options** - 30min, 1hr, 3hr, or indefinite pause
- **One-click remote reboot** with safety confirmation
- **Live system metrics** (uptime, memory, connectivity status)
- Auto-resume alerts when DNS recovers

**Alert Control Endpoints:**

- `/alerts/pause/30` - Pause alerts for 30 minutes
- `/alerts/pause/60` - Pause alerts for 1 hour
- `/alerts/pause/180` - Pause alerts for 3 hours
- `/alerts/pause/indefinite` - Pause alerts indefinitely
- `/alerts/resume` - Resume alerts manually

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
3. **Build and upload with monitoring**: `./upload_and_monitor.sh`
   - *Or use standard PlatformIO*: `platformio run --target upload` (auto-monitors)
4. **Access web interface**: `http://poop-monitor.local/`

### Development Workflow

- **Rapid deployment**: `./upload_and_monitor.sh` (builds, uploads, monitors with smart detection)
- **Status monitoring**: `./monitor_esp32.sh` (comprehensive diagnostics with formatted metrics)
- **Remote management**: `./reboot_esp32.sh` (safe remote reboot with verification)
- **Quick access**: Use `--telnet` or `--web` flags for direct connections

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

## Recent Updates & Code Improvements

### Version 2.1.0 - Smart DNS Monitoring & Code Readability

- **🕐 Smart DNS alerting**: 5-minute delay before first alert, 30-minute intervals for subsequent alerts
- **🔍 Duplicate server detection**: Skips critical alerts when primary/fallback DNS are identical
- **🔕 Manual alert control**: Web-based pause/resume with flexible timing options (30min, 1hr, 3hr, indefinite)
- **🔄 Auto-resume on recovery**: Alerts automatically resume when DNS recovers
- **🏗️ Improved code organization**: Descriptive function names and better separation of concerns
- **📝 Enhanced documentation**: Detailed DNS monitoring section and updated feature descriptions
- **🔧 Better constants naming**: Clear `_MS` suffix for millisecond values and descriptive variable names

### Version 1.4.0 - Enhanced Automation & Code Optimization

- **🔧 Reduced code duplication**: Created centralized `formatUptime()` utility function
- **📊 Enhanced status endpoint**: Improved JSON API with formatted timestamps  
- **🎯 Streamlined monitoring**: Eliminated duplicate JSON parsing in scripts
- **⚡ Smart device detection**: Enhanced post-upload monitoring with automatic ping verification
- **🛠️ Better error handling**: Improved resilience and user feedback across all scripts
- **📋 Comprehensive documentation**: Updated README with detailed feature descriptions

### Technical Improvements

- **Smart DNS timing logic** prevents notification spam while ensuring important alerts
- **Modular function design** with clear responsibility separation
- **Centralized uptime formatting** in `system_utils.cpp` eliminates duplicate code
- **Shared JSON parsing function** in monitoring scripts reduces redundancy
- **Enhanced status API** provides formatted timestamps and human-readable uptime
- **Optimized script workflow** with better error handling and user guidance

## Version History

- **v2.1.0**: Smart DNS alerting with 5-minute delay and 30-minute intervals, improved code readability with descriptive function names, duplicate DNS server detection
- **v1.4.0**: Enhanced automation, code optimization, improved monitoring with formatted timestamps
- **v1.3.0**: Added web interface, remote reboot, comprehensive monitoring scripts
- **v1.2.0**: Modular code structure with security improvements  
- v1.1.1: Added Pushover credentials
- v1.1.0: Added telnet server functionality
- v1.0.1: Initial version with basic monitoring

## Security Notes

- Never commit `credentials.cpp` - it's in `.gitignore`
- Use strong passwords for OTA updates
- Consider using WPA3 for WiFi if available
- Pushover tokens should be kept secret
