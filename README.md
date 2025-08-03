# ESP32 Poop Monitor

An ESP32-based monitoring device that sends heartbeat requests and provides remote telnet access.

## Features

- 📡 WiFi connectivity with custom DNS configuration
- 🔄 OTA (Over-The-Air) firmware updates
- 📱 Pushover notifications for alerts
- 🖥️ Telnet server for remote monitoring
- 🔍 DNS resolution testing with fallback
- 💾 Persistent version tracking

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
└── ota_manager.h/.cpp    # OTA updates
```

## Building and Deploying

1. **Build**: `pio run`
2. **Upload via USB**: `pio run --target upload`
3. **Upload via OTA**: `pio run --target upload` (after initial USB upload)

## Remote Monitoring

Connect via telnet to see live logs:
```bash
telnet <ESP32_IP> 23
# or
telnet poop-monitor.local 23
```

## Version History

- v1.2.0: Modular code structure with security improvements
- v1.1.1: Added Pushover credentials
- v1.1.0: Added telnet server functionality
- v1.0.1: Initial version with basic monitoring

## Security Notes

- Never commit `credentials.cpp` - it's in `.gitignore`
- Use strong passwords for OTA updates
- Consider using WPA3 for WiFi if available
- Pushover tokens should be kept secret
