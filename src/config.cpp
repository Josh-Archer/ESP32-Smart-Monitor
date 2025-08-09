#include "config.h"
#include "credentials.h"

// Firmware version - increment this with each update
const char* firmwareVersion = "2.4.0";

// WiFi Configuration (from credentials.h)
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// API Configuration
const char* apiEndpoint = "http://notifications.archerfamily.io/heartbeat/poop";
const char* otaPassword = OTA_PASSWORD;
const char* deviceName = "poop-monitor";

// DNS Configuration
IPAddress primaryDNS(192, 168, 68, 51);    // Your custom DNS server
IPAddress fallbackDNS(192, 168, 68, 51);         

// Pushover Configuration (from credentials.h)
const char* pushoverToken = PUSHOVER_TOKEN;
const char* pushoverUser = PUSHOVER_USER;
const char* pushoverApiUrl = "https://api.pushover.net/1/messages.json";

// MQTT Configuration
const char* mqttServer = "homeassistant.local";      // Your MQTT broker IP
const int mqttPort = 1883;                     // MQTT port (1883 or 8883 for SSL)
const char* mqttUser = MQTT_USER;                     // MQTT username (empty if no auth)
const char* mqttPassword = MQTT_PASSWORD;                 // MQTT password (empty if no auth)
