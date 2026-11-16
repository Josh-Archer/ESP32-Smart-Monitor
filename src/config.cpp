#include "config.h"
#include "credentials.h"

#if !defined(UNIT_TEST) && !defined(MOCK_ARDUINO)
#if !__has_include("credentials.cpp") && !__has_include("src/credentials.cpp")
#error "src/credentials.cpp is missing! Copy credentials.template.cpp to src/credentials.cpp and set your Wi-Fi and OTA credentials."
#endif
#endif

const char* firmwareVersion = "5.0.0";

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
// AdGuard's floating address on the Homelab VLAN. It does not belong to a
// node, so it stays the same when nodes move (Josh-Archer/home#4399). It was
// 192.168.68.94, a node's own address, which went away on 2026-11-16.
IPAddress primaryDNS(172, 16, 20, 53);     // Active network DNS server
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
