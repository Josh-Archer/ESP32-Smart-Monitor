#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <IPAddress.h>

// Firmware version - increment this with each update
extern const char* firmwareVersion;

// WiFi Configuration
extern const char* ssid;
extern const char* password;

// API Configuration
extern const char* apiEndpoint;
extern const char* otaPassword;
extern const char* deviceName;

// DNS Configuration
extern IPAddress primaryDNS;
extern IPAddress fallbackDNS;
extern IPAddress secondaryFallback;

// Pushover Configuration
extern const char* pushoverToken;
extern const char* pushoverUser;
extern const char* pushoverApiUrl;

#endif
