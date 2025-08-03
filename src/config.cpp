#include "config.h"
#include "credentials.h"

// Firmware version - increment this with each update
const char* firmwareVersion = "1.2.0";

// WiFi Configuration (from credentials.h)
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// API Configuration
const char* apiEndpoint = "http://notifications.archerfamily.io/heartbeat/poop";
const char* otaPassword = OTA_PASSWORD;
const char* deviceName = "poop-monitor";

// DNS Configuration
IPAddress primaryDNS(192, 168, 68, 51);    // Your custom DNS server
IPAddress fallbackDNS(1, 1, 1, 1);         
IPAddress secondaryFallback(8, 8, 4, 4);   // Google DNS secondary

// Pushover Configuration (from credentials.h)
const char* pushoverToken = PUSHOVER_TOKEN;
const char* pushoverUser = PUSHOVER_USER;
const char* pushoverApiUrl = "https://api.pushover.net/1/messages.json";
