# Scripts Directory

This directory contains automation scripts for ESP32 development and deployment.

## Files

### `scripts.ps1` - PowerShell Development Scripts
**Platform:** Windows PowerShell
**Purpose:** Comprehensive development workflow automation

**Usage:**
```powershell
# Load scripts in current session
. .\scripts\scripts.ps1

# See all available commands
pio-help
```

**Key Functions:**
- `pio-upload-ota` - Upload firmware via OTA
- `deploy-ota` - Build + Upload + Monitor workflow
- `telnet-monitor` - Connect to ESP32 telnet server
- `ping-device` - Test device connectivity
- `commit-version` - Git commit with version tagging

## Platform-Specific Scripts

### Windows (PowerShell)
- **Location:** `scripts/scripts.ps1`
- **Setup:** `. .\scripts\scripts.ps1`
- **Features:** OTA deployment, telnet monitoring, git workflows

### Linux/macOS (Bash)
- **Location:** Project root (various `.sh` files)
- **Examples:** `monitor_esp32.sh`, `reboot_esp32.sh`, `upload_and_monitor.sh`
- **Features:** Device monitoring, remote reboot, comprehensive diagnostics

## Adding New Scripts

When adding new scripts:
1. Place platform-specific scripts in this directory
2. Update this README with descriptions
3. Update main project README with usage examples
4. Consider cross-platform compatibility where possible

## Configuration

Edit `scripts.ps1` to modify:
- Device IP address (`$DEVICE_IP`)
- Device hostname (`$DEVICE_HOSTNAME`) 
- PlatformIO path (`$PIO_PATH`)
