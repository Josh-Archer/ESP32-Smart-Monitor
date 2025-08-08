# Recent Changes Summary

## Progressive Web App & Bootstrap Interface (v2.3.0)

Complete modernization with Bootstrap CSS framework, live telnet console streaming, and full Progressive Web App (PWA) implementation enabling app-like experience with offline functionality.

**Bootstrap Web Interface**:

- **Replaced custom CSS with Bootstrap 5.3.0** for professional design
- Responsive card-based layout with icons from Bootstrap Icons
- Modern navbar with connection status indicator
- Toast notifications for user feedback
- Modal dialogs for confirmations
- Mobile-first responsive design

**Live Console Feature**:

- **Real-time telnet log streaming** to web interface
- Console window with syntax highlighting
- Auto-scroll functionality with toggle control
- Log level detection (error, warning, info, success)
- Terminal-style console display with scrollbars
- Clear console and manual scroll controls

**Progressive Web App (PWA) Features**:

- **Progressive Web App manifest** with app metadata and icons
- **Service Worker implementation** for offline functionality and caching
- **App installation** - Install as native app on mobile/desktop
- **Offline support** - Cached interface and data when device unreachable
- **App shortcuts** - Quick access to status and reboot functions
- **Background sync** - Ready for future push notification features

**Enhanced Offline Experience**:

- **Smart caching strategy** - Static files cached for offline use
- **Offline page** with beautiful design and retry functionality
- **Cached device status** - Show last known data when offline
- **Auto-reconnection** - Automatic retry when connection restored
- **Connection indicators** - Visual feedback for online/offline state

**Installation & App Features**:

- **One-click install** - "Install App" button appears on compatible browsers
- **Standalone mode** - Runs like a native app without browser chrome
- **App shortcuts** - Access device status and reboot directly from launcher
- **Theme integration** - Matches Bootstrap design with proper branding
- **Mobile optimized** - Perfect mobile app experience

**File Structure Improvements**:

- `/data/index.html` - Bootstrap-based main interface with PWA support
- `/data/style.css` - Custom CSS for console and enhancements
- `/data/app.js` - Comprehensive JavaScript application class with PWA functionality
- `/data/manifest.json` - PWA app manifest for installation
- `/data/sw.js` - Service Worker for offline functionality
- `/data/offline.html` - Beautiful offline fallback page
- Static file serving for all assets via dedicated routes

**Technical Enhancements**:

- Telnet log buffer system for web streaming
- JSON escaping for safe log transmission
- Connection status monitoring with visual indicators
- Advanced error handling and user experience improvements
- Keyboard shortcuts (Ctrl+R refresh, Ctrl+` toggle console)
- Service Worker with intelligent caching strategies
- PWA manifest with proper metadata and icon definitions
- LocalStorage integration for offline data persistence
- Background sync preparation for future features
- Enhanced error handling for offline scenarios

---

## Static Web Interface & Memory Optimization (v2.2.0)

Major refactoring to move HTML/CSS/JavaScript to static files, dramatically reducing flash memory usage and improving web interface performance.

**Memory Optimization Results**:

- **Flash usage reduced from 76.6% to 52.9%** (23.7% reduction!)
- Modern JavaScript-based interface with auto-refresh
- Responsive design with improved visual styling
- SPIFFS filesystem integration for static file serving

**Web Interface Improvements**:

- Separated HTML/CSS/JavaScript into `/data/index.html` and `/data/reboot.html`
- Dynamic content loading via JavaScript fetch API
- Auto-refresh every 30 seconds for live status updates
- Improved visual design with grid layout and better typography
- Real-time WiFi signal strength display
- Enhanced error handling and user feedback
- Mobile-responsive design

**Technical Changes**:

- Added SPIFFS filesystem support in `platformio.ini`
- Modified web server to serve static files from SPIFFS
- Simplified C++ handlers to minimal JSON API responses
- Reduced string literals in firmware code significantly
- Enhanced status endpoint with formatted uptime display

**Files Added**:

- `data/index.html` - Main control panel interface
- `data/reboot.html` - Reboot confirmation page

**Code Changes**:

- Updated `src/web_server.cpp` to use SPIFFS file serving
- Modified `platformio.ini` to include SPIFFS partition configuration
- Version bumped to 2.2.0

## Smart Recovery Alerts (v2.1.2)

Fixed DNS recovery alerts to follow the same 5-minute threshold timing as failure alerts. Recovery notifications now only trigger after DNS has been stable for 5+ minutes, preventing premature recovery alerts during intermittent connectivity issues.

**Changes**:

- Added `DNS_RECOVERY_THRESHOLD_MS` constant (5 minutes)
- Recovery alerts now wait for 5 minutes of stable DNS before notifying
- Added recovery time tracking with `dnsRecoveryTime` variable
- Recovery alerts respect alert pause settings
- Enhanced logging shows countdown to recovery alert

## Fix Icon Render (v2.1.1)

Notification HTML Icon wasn't rendering. Switched to HTML entity codes vs emojii.

## Smart DNS Monitoring Release (v2.1.0) - Intelligent Alerting & Code Improvements

This release introduces smart DNS monitoring with configurable timing, improved code readability, and enhanced user experience through better function organization.

## NEW FEATURES IN v2.1.0

### 1. Smart DNS Alerting System

**ENHANCED MODULE**: `src/dns_manager.h/cpp`

- **5-Minute Alert Delay**: Prevents alerts for temporary network hiccups
- **30-Minute Alert Intervals**: Reduces notification spam while keeping you informed
- **Duplicate Server Detection**: Skips critical alerts when primary/fallback are the same
- **Optimized Fallback Testing**: Skips fallback DNS testing when servers are identical
- **Detailed Timing Logs**: Tracks failure duration and alert timing

**Smart Logic Features**:

- First alert only after DNS has been down for 5+ minutes
- Subsequent alerts every 30 minutes while DNS remains down
- Immediate recovery notifications when DNS comes back online
- Intelligent handling of identical primary/fallback DNS servers
- No unnecessary fallback testing when primary and fallback are the same

### 2. Manual Alert Control System

**NEW WEB INTERFACE FEATURES**: Enhanced web control panel

- **Flexible Pause Options**: 30 minutes, 1 hour, 3 hours, or indefinite pause
- **Real-time Alert Status**: Visual indicator of alert state on web interface
- **Auto-Resume on Recovery**: Alerts automatically resume when DNS recovers
- **Manual Resume**: One-click resume from web interface
- **Smart Timing Preservation**: Maintains alert intervals when paused/resumed

**Web Interface Enhancements**:

- Styled control panel with clear visual indicators
- Alert status display with countdown for timed pauses
- Dedicated alert control section with intuitive buttons
- Confirmation dialogs for important actions
- Responsive design with color-coded status indicators

**New API Endpoints**:

- `/alerts/pause/30` - Pause for 30 minutes
- `/alerts/pause/60` - Pause for 1 hour  
- `/alerts/pause/180` - Pause for 3 hours
- `/alerts/pause/indefinite` - Pause indefinitely
- `/alerts/resume` - Resume alerts manually

### 3. Improved Code Readability & Organization

**REFACTORED FUNCTIONS**: Better naming and structure

- `testDNSResolutionWithSmartAlerting()` - Main DNS testing function
- `handleSuccessfulDNSResolution()` - Recovery logic
- `handlePrimaryDNSFailureWithFallback()` - Fallback handling
- `shouldSendDNSDownAlert()` - Alert timing logic
- `sendDNSDownAlert()` - Alert formatting and sending
- `resetDNSFailureTracking()` - State reset utility

**Code Quality Improvements**:

- Descriptive function names that explain their purpose
- Clear separation of concerns with dedicated helper functions
- Improved variable naming with meaningful prefixes
- Better documentation and inline comments

### 4. Enhanced Configuration & Constants

**IMPROVED TIMING CONSTANTS**:

- `DNS_FAILURE_THRESHOLD_MS` - 5 minutes before first alert
- `DNS_ALERT_INTERVAL_MS` - 30 minutes between alerts
- Clear naming convention with `_MS` suffix for milliseconds

## PREVIOUS MAJOR FEATURES (v2.0.0 & Earlier)

### 1. Web Server & Control Panel

**NEW MODULE**: `src/web_server.h/cpp`

- **HTTP Control Panel**: Access at `http://poop-monitor.local/`
- **JSON Status API**: Comprehensive device metrics at `/status` endpoint
- **Remote Reboot**: One-click reboot with safety confirmation at `/reboot`
- **Real-time Monitoring**: Live system metrics, DNS status, heartbeat tracking

**Features**:

- Device information display (name, version, IP, uptime)
- WiFi signal strength and connection status
- DNS configuration monitoring (primary vs current servers)
- Heartbeat tracking with formatted timestamps
- Memory usage and system diagnostics

### 2. System Utilities & Device Management

**NEW MODULE**: `src/system_utils.h/cpp`

- **Safe Device Reboots**: `rebootDevice()` with logging and delay
- **Remote Reboot Flags**: `setRebootFlag()` for web-triggered reboots
- **Uptime Formatting**: `formatUptime()` utility for consistent time displays
- **Persistent Logging**: Reboot reasons stored in preferences

**Capabilities**:

- Clean device restarts with proper logging
- Web interface integration for remote management
- Consistent uptime formatting across all interfaces

### 3. Comprehensive Monitoring Scripts

#### `monitor_esp32.sh` - Advanced Device Diagnostics

**NEW SCRIPT**: Complete device health monitoring

- **Network Testing**: Ping connectivity, service availability
- **DNS Monitoring**: Primary vs fallback server usage tracking
- **Heartbeat Analysis**: Success timestamps, response codes, time tracking
- **Formatted Output**: Human-readable uptime, colored status indicators
- **Interactive Options**: `--telnet` and `--web` flags for direct access

#### `reboot_esp32.sh` - Safe Remote Reboot Management

**NEW SCRIPT**: Intelligent device reboot system

- **Safety Confirmations**: Interactive prompts (bypass with `--force`)
- **Reboot Monitoring**: Tracks offline/online status during reboot
- **Service Verification**: Confirms web server availability post-reboot
- **Status Reporting**: Version and uptime display after successful reboot

#### `upload_and_monitor.sh` - All-in-One Deployment

**NEW SCRIPT**: Complete build-to-monitoring workflow

- **Automated Build**: PlatformIO compilation and upload
- **Smart Device Detection**: Waits for device initialization
- **Post-Upload Monitoring**: Automatic status verification
- **User Guidance**: Clear next steps and available commands

### 4. PlatformIO Integration & Automation

**NEW SCRIPT**: `scripts/post_upload.py`

- **Automatic Monitoring**: Triggers monitoring after successful uploads
- **Smart Waiting**: Device initialization detection
- **Error Handling**: Timeout and failure management
- **User Feedback**: Clear status reporting and next steps

**Configuration Updates**:

- Added WebServer library dependency
- PlatformIO post-upload script integration
- Enhanced project description and metadata

## ENHANCED CORE FUNCTIONALITY

### 5. Advanced Heartbeat Tracking

**NEW GLOBALS**: `lastSuccessfulHeartbeat`, `lastHeartbeatResponseCode`

- **Success Tracking**: Timestamps of successful heartbeat responses
- **Response Monitoring**: HTTP status code tracking
- **Time Calculations**: Seconds since last success
- **Formatted Displays**: Human-readable success timestamps

### 6. Enhanced Status API

**NEW JSON FIELDS**:

- `last_heartbeat_success`: Timestamp of last successful heartbeat
- `last_heartbeat_code`: HTTP response code from last attempt
- `time_since_last_success_seconds`: Time since last success in seconds
- `last_heartbeat_uptime_formatted`: Human-readable success time
- `current_uptime_formatted`: Current device uptime display
- `current_dns1/dns2`: Active DNS servers in use
- `primary_dns/fallback_dns`: Configured DNS servers

## CODE QUALITY IMPROVEMENTS

### 7. Eliminated Code Duplication

**Centralized Uptime Formatting**:

- Created `formatUptime()` utility function
- Eliminated duplicate uptime calculations in web server
- Consistent formatting across all interfaces
- Reduced code by ~15 lines in single module

**Streamlined Monitoring Scripts**:

- Created `display_device_metrics()` function
- Eliminated ~80 lines of duplicate JSON parsing
- Consistent output formatting
- Easier maintenance and updates

### 8. Enhanced Architecture

**Modular Design**:

- Clear separation of concerns across modules
- Centralized configuration in `config.h/cpp`
- Consistent error handling and logging
- Improved code organization

## CONFIGURATION & INFRASTRUCTURE

### 9. Version Management

- **Version Bump**: Updated to 2.0.0 reflecting major changes
- **Persistent Tracking**: Enhanced version logging and update detection
- **OTA Integration**: Improved over-the-air update handling

### 10. Documentation Overhaul

**Enhanced README.md**:

- Comprehensive feature documentation
- Detailed script usage examples
- Troubleshooting guide with specific solutions
- Development workflow documentation
- Quick start guide for new users

## FILES ADDED/MODIFIED

### New Files Created

- `src/web_server.h/cpp` - Web interface and JSON API
- `src/system_utils.h/cpp` - Device management utilities
- `monitor_esp32.sh` - Advanced monitoring script
- `reboot_esp32.sh` - Remote reboot management
- `upload_and_monitor.sh` - Deployment automation
- `scripts/post_upload.py` - PlatformIO integration

### Modified Files

- `src/main.cpp` - Web server integration, heartbeat tracking
- `src/config.cpp` - Version bump to 2.0.0
- `platformio.ini` - WebServer library, post-upload scripts
- `README.md` - Comprehensive documentation updates
- `.vscode/launch.json` - Debug configuration updates

### Legacy Files Removed

- `esp32/esp32.cpp` - Replaced by modular architecture
- `esp32/esp32.ino` - Superseded by main.cpp enhancements

## TESTING & VERIFICATION

### Build Verification

- ✅ `pio run` completes successfully
- ✅ All dependencies resolved
- ✅ No compilation errors

### Functionality Testing

- ✅ Web interface operational
- ✅ Remote reboot functionality working
- ✅ Monitoring scripts functional
- ✅ JSON API providing comprehensive data
- ✅ Automated upload monitoring working

### Feature Validation

- ✅ DNS monitoring tracking primary vs fallback
- ✅ Heartbeat tracking with formatted timestamps
- ✅ Uptime formatting consistent across interfaces
- ✅ Error handling and user feedback improved

## IMPACT SUMMARY

This release represents a complete transformation from a basic monitoring device to a comprehensive IoT management platform:

1. **User Experience**: Web interface eliminates need for command-line access
2. **Automation**: Complete build-to-monitoring workflows
3. **Monitoring**: Advanced diagnostics with formatted output
4. **Management**: Safe remote device control and rebooting
5. **Maintainability**: Centralized utilities and reduced duplication
6. **Documentation**: Comprehensive guides for all skill levels

The ESP32 device now provides enterprise-level monitoring and management capabilities while maintaining the simplicity of the original heartbeat functionality.
