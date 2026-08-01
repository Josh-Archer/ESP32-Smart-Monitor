#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <IPAddress.h>

// Firmware version - increment this with each update
extern const char* firmwareVersion;

// WiFi Configuration
extern const char* ssid;
extern const char* password;

// Heartbeat / notification-api configuration
// Full URL is resolved at runtime by getHeartbeatEndpoint():
//   - if heartbeatPath is non-empty: {heartbeatBaseUrl}{heartbeatPath}
//   - else: {heartbeatBaseUrl}/heartbeat/{heartbeatDeviceId}
// See docs/HEARTBEAT.md for the notification-api contract and examples.
extern const char* heartbeatBaseUrl;   // no trailing slash
extern const char* heartbeatDeviceId;  // path segment for notification-api
extern const char* heartbeatPath;      // optional full path override (e.g. "/health")
const char* getHeartbeatEndpoint();    // resolved URL used for the periodic GET

// Device / OTA
extern const char* otaPassword;
extern const char* deviceName;

// DNS Configuration
extern IPAddress primaryDNS;
extern IPAddress fallbackDNS;

// Pushover Configuration
extern const char* pushoverToken;
extern const char* pushoverUser;
extern const char* pushoverApiUrl;

// MQTT Configuration
extern const char* mqttServer;
extern const int mqttPort;
extern const char* mqttUser;
extern const char* mqttPassword;

#endif
