#include "config.h"
#include "credentials.h"  // secrets: src/credentials_private.h (from credentials.template.h)

const char* firmwareVersion = "2.9.0";

// --- Secrets (from credentials macros; never commit real values) ---
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* otaPassword = OTA_PASSWORD;
const char* pushoverToken = PUSHOVER_TOKEN;
const char* pushoverUser = PUSHOVER_USER;
const char* mqttUser = MQTT_USER;
const char* mqttPassword = MQTT_PASSWORD;

// --- Non-secret device config (safe to commit; edit for your network) ---
const char* apiEndpoint = "http://notifications.archerfamily.io/heartbeat/poop";
const char* deviceName = "poop-monitor";

// DNS Configuration
IPAddress primaryDNS(192, 168, 68, 51);    // Your custom DNS server
IPAddress fallbackDNS(192, 168, 68, 51);

// Pushover API (public endpoint; tokens stay in credentials_private.h)
const char* pushoverApiUrl = "https://api.pushover.net/1/messages.json";

// MQTT broker (host/port are not secrets; auth is via MQTT_USER / MQTT_PASSWORD)
const char* mqttServer = "homeassistant.local";  // Your MQTT broker hostname or IP
const int mqttPort = 1883;                       // 1883 or 8883 for SSL
