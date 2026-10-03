#include "config.h"
#include "credentials.h"

const char* firmwareVersion = "4.0.4";

// WiFi Configuration (from credentials.h)
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* ssidSecondary = WIFI_SSID_SECONDARY;
const char* passwordSecondary = WIFI_PASSWORD_SECONDARY;

// API Configuration
const char* apiEndpoint = "http://notifications.archerfamily.io/heartbeat/poop";
const char* otaPassword = OTA_PASSWORD;
const char* deviceName = "poop-monitor";

// DNS Configuration
IPAddress primaryDNS(192, 168, 68, 94);    // Active network DNS server
IPAddress fallbackDNS(1, 1, 1, 1);         // Public fallback DNS (Cloudflare)

// Pushover Configuration (from credentials.h)
const char* pushoverToken = PUSHOVER_TOKEN;
const char* pushoverUser = PUSHOVER_USER;
const char* pushoverApiUrl = "https://api.pushover.net/1/messages.json";

// MQTT Configuration
const char* mqttServer = "192.168.1.150";           // Home Assistant IP (was homeassistant.local)
const int mqttPort = 1883;                     // MQTT port (1883 or 8883 for SSL)
const char* mqttUser = MQTT_USER;                     // MQTT username (empty if no auth)
const char* mqttPassword = MQTT_PASSWORD;                 // MQTT password (empty if no auth)
