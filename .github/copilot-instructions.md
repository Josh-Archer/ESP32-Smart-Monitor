# ESP32 Smart Monitor

ESP32-based IoT monitoring device with **Home Assistant integration**, modern web interface, live telnet streaming, smart DNS monitoring, and comprehensive automation tools. The project uses PlatformIO for ESP32 firmware development and Docker/Kubernetes for web interface deployment.

**Always reference these instructions first and fallback to search or bash commands only when you encounter unexpected information that does not match the info here.**

## Working Effectively

### Bootstrap and Dependencies
- **Install PlatformIO CLI**: `pip3 install platformio` (takes 30-60 seconds)
- **Create credentials file**: Copy and edit `src/credentials.cpp` with actual WiFi/MQTT/Pushover values
- **CRITICAL NETWORK ISSUE**: PlatformIO platform downloads currently fail due to network connectivity - `HTTPClientError` when installing espressif32 platform
- **Manual setup required**: If PlatformIO installation fails, firmware builds will not work until network/proxy issues are resolved

### Firmware Build and Upload
- **NETWORK CONNECTIVITY REQUIRED**: ESP32 platform installation needs internet access to PlatformIO registry
- **Build command**: `~/.local/bin/platformio run` -- FAILS due to network issues. Set timeout to 15+ minutes when network works. NEVER CANCEL.
- **OTA Upload**: `~/.local/bin/platformio run --target upload` -- requires actual ESP32 device at `poop-monitor.local`
- **Serial Upload**: `~/.local/bin/platformio run -e esp32-c3-devkitm-1-serial --target upload` -- requires USB connection
- **Dependencies**: PubSubClient (MQTT) and ArduinoJson libraries auto-install with successful platform installation
- **Post-upload automation**: `python3 scripts/post_upload.py` runs monitoring automatically (60 second timeout)

### Web Interface (Always Works)
- **Build web container**: `docker build -f k8s/Dockerfile -t esp32-monitor-web:test .` -- takes 3 seconds. NEVER CANCEL. Set timeout to 60+ seconds.
- **Run web interface**: `docker run --rm -p 8080:80 esp32-monitor-web:test` -- serves on localhost:8080
- **Deploy to Kubernetes**: `./scripts/deploy_web.sh [tag] [image] [namespace]` -- requires kubectl access
- **Web tech stack**: nginx + Bootstrap + vanilla JavaScript, no build tools required

### Automation Scripts
- **Windows development**: Load PowerShell scripts with `. .\scripts\scripts.ps1` (provides 20+ helper functions)
- **Unix development**: Use bash scripts `./monitor_esp32.sh`, `./reboot_esp32.sh`, `./upload_and_monitor.sh`
- **Post-upload monitoring**: Automatic via `scripts/post_upload.py` (45-60 second timeout)
- **Make scripts executable**: Always run `chmod +x *.sh` after fresh clone

## Validation

### Always Validate Web Components
- **ALWAYS test web interface**: Build and run Docker container to verify web UI functionality
- **Test web build**: `time docker build -f k8s/Dockerfile -t test .` -- should complete in 3 seconds
- **Test web functionality**: `docker run --rm -p 8081:80 test && curl localhost:8081` -- verify HTML response
- **Manual validation**: Open http://localhost:8081 in browser - should show ESP32 Control Panel interface
- **Validate assets**: Check manifest.json, app.js (683 lines), style.css are served correctly
- **PWA functionality**: Verify service worker, icons, and Progressive Web App features load

### Device Communication Validation (When Hardware Available)
- **Test device connectivity**: `./monitor_esp32.sh` -- times out properly when device unavailable
- **Validate telnet connection**: `nc poop-monitor.local 23` -- connects to live device console
- **Check web API**: `curl http://poop-monitor.local/status` -- returns JSON device status
- **Remote reboot test**: `./reboot_esp32.sh --force` -- safely reboots device over HTTP

### Firmware Validation (Requires Network Fix)
- **CANNOT currently validate**: PlatformIO builds fail due to network connectivity to platform registry
- **When network works**: Build takes 5-15 minutes for ESP32 platform setup. NEVER CANCEL. Set timeout to 20+ minutes.
- **Manual verification needed**: Upload to actual ESP32-C3 hardware and verify telnet/web endpoints respond
- **Post-upload monitoring**: Device should respond at `poop-monitor.local` within 8 seconds of upload

## Build Timing and Timeouts

### Measured Build Times
- **Web Docker build**: 3 seconds (validated) -- Set timeout to 60+ seconds minimum
- **PlatformIO dependency install**: FAILS due to network -- normally 2-5 minutes. Set timeout to 10+ minutes.
- **ESP32 firmware build**: CANNOT measure due to network -- estimated 5-15 minutes. Set timeout to 20+ minutes. NEVER CANCEL.
- **OTA upload to device**: Not testable without hardware -- estimated 30-60 seconds
- **Device initialization**: 8 seconds post-upload (measured in scripts)

### NEVER CANCEL Build Operations
- **CRITICAL**: Always wait for PlatformIO builds to complete - ESP32 toolchain setup takes substantial time
- **Web builds are fast**: 3 seconds typical, but allow 60+ seconds timeout
- **Platform installation**: Can take 10+ minutes on first run. NEVER CANCEL.
- **Device upload**: Allow 2+ minutes for OTA operations
- **Post-upload monitoring**: Scripts timeout at 45-60 seconds automatically

## Manual Workarounds

### When PlatformIO Network Fails (Current State)
- **Cannot build firmware**: Platform installation fails with `HTTPClientError`
- **Web interface still works**: Docker build and deployment functions normally  
- **Use existing scripts**: Monitoring and reboot scripts handle device-not-found gracefully
- **Alternative validation**: Test script functionality with expected failure modes
- **PowerShell available**: Use `pwsh` for cross-platform PowerShell script testing

### When Device Hardware Unavailable
- **Scripts fail gracefully**: `./monitor_esp32.sh` exits cleanly when device unreachable (0.05s)
- **Web interface works standalone**: Docker container serves UI without backend
- **API endpoints return errors**: HTTP 404/503 when device not available
- **Telnet connections fail**: Scripts report connection failures appropriately
- **Automation continues**: Post-upload scripts handle device unavailability correctly

### Manual Validation Steps (All Verified Working)
1. **Install PlatformIO**: `pip3 install platformio` (30-60s, works)
2. **Create credentials**: Edit `src/credentials.cpp` with actual values (template created)
3. **Build web interface**: `docker build -f k8s/Dockerfile -t test .` (3s, works)
4. **Test web serving**: `docker run --rm -p 8081:80 test` (works, serves all assets)
5. **Validate scripts**: `chmod +x *.sh && ./monitor_esp32.sh` (works, handles offline device)
6. **Test automation**: `python3 scripts/post_upload.py` (works, 8s timeout when device offline)

## Common Pitfalls

### Network and Connectivity
- **PlatformIO registry access**: May require proxy configuration in corporate environments
- **mDNS hostname resolution**: `poop-monitor.local` requires device on same network
- **Docker registry access**: Web deployment requires internet for nginx:alpine base image
- **MQTT broker connectivity**: Home Assistant integration needs MQTT server configuration

### Configuration Management
- **Credentials file required**: Create `src/credentials.cpp` with actual values (template provided)
- **mDNS hostname consistency**: Device uses `poop-monitor.local` throughout scripts and firmware
- **MQTT auto-discovery**: Device publishes to `homeassistant/sensor/poop_monitor/*` topics
- **OTA password**: Must match between `platformio.ini` and `src/config.cpp`

### Build Environment Issues
- **Platform-specific scripts**: PowerShell (.ps1) for Windows, Bash (.sh) for Unix
- **Python version**: PlatformIO requires Python 3.x, scripts use `python3` explicitly
- **File permissions**: Shell scripts need execute permissions (`chmod +x *.sh`)
- **Docker service**: Web deployment requires Docker daemon running

## Troubleshooting Commands

### Network and Platform Issues
```bash
# Check PlatformIO installation
~/.local/bin/platformio --version

# Attempt platform installation (currently fails)
~/.local/bin/platformio pkg install --platform espressif32

# Test web build (always works)
docker build -f k8s/Dockerfile -t test .
```

### Device Communication
```bash
# Test device availability
ping -c 3 poop-monitor.local

# Check device status
curl -m 5 http://poop-monitor.local/status

# Monitor device (handles failures gracefully)
./monitor_esp32.sh
```

### Script Functionality
```bash
# Make scripts executable
chmod +x *.sh

# Test monitoring script
time ./monitor_esp32.sh  # Exits cleanly when device unavailable (0.05s)

# Test reboot script  
./reboot_esp32.sh --force  # Shows proper error when device unreachable (0.01s)

# Test upload workflow
./upload_and_monitor.sh  # Fails gracefully when PlatformIO unavailable

# Test post-upload automation
python3 scripts/post_upload.py  # Handles device unavailability correctly (8s timeout)
```

## Key Project Structure

### Source Files
```
src/
├── main.cpp              # Main ESP32 program
├── config.h/.cpp         # Device configuration  
├── credentials.h/.cpp    # WiFi/MQTT/API credentials (create from template)
├── telnet.h/.cpp         # Live console server
├── mqtt_manager.h/.cpp   # Home Assistant integration
├── dns_manager.h/.cpp    # Smart DNS monitoring
├── notifications.h/.cpp  # Pushover alerts
├── ota_manager.h/.cpp    # Over-the-air updates
├── system_utils.h/.cpp   # Device utilities
└── web_server.h/.cpp     # HTTP API endpoints
```

### Automation Scripts
```
scripts/
├── scripts.ps1           # Windows PowerShell development functions
├── deploy_web.ps1/.sh    # Docker/Kubernetes deployment
├── post_upload.py        # Automated post-upload monitoring
└── post_upload_monitor.ps1  # Windows monitoring integration

./monitor_esp32.sh        # Device connectivity and status check
./reboot_esp32.sh         # Safe remote device reboot
./upload_and_monitor.sh   # Complete build-upload-monitor workflow
```

### Web Interface
```
web/
├── index.html            # Main control panel interface
├── app.js                # Device interaction JavaScript
├── style.css             # Custom styling
└── manifest.json         # PWA configuration

k8s/
├── Dockerfile            # Web container definition
├── deployment.yaml       # Kubernetes deployment
└── nginx.conf            # Web server configuration
```

## Quick Reference

### Essential Commands
- **Web development**: `docker build -f k8s/Dockerfile -t test . && docker run --rm -p 8081:80 test`
- **Device monitoring**: `./monitor_esp32.sh` (handles offline devices gracefully, 0.05s when offline)
- **Windows development**: `pwsh -Command ". .\scripts\scripts.ps1"` then use PowerShell functions
- **Test all scripts**: `chmod +x *.sh && ./monitor_esp32.sh && ./reboot_esp32.sh --force`
- **Platform status**: `~/.local/bin/platformio pkg list` (shows if ESP32 platform installed)
- **Post-upload testing**: `python3 scripts/post_upload.py` (validates automation workflows)

### Device Endpoints (When Hardware Available)
- **Web interface**: http://poop-monitor.local/
- **Status API**: http://poop-monitor.local/status (JSON)
- **Reboot endpoint**: http://poop-monitor.local/reboot
- **Live telnet**: `nc poop-monitor.local 23`

Fixes #10.