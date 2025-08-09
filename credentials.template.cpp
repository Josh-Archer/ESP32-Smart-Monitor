#include "credentials.h"

// INSTRUCTIONS:
// 1. Copy this file to src/credentials.cpp
// 2. Fill in your actual values
// 3. credentials.cpp is in .gitignore so your secrets won't be committed

// WiFi Configuration
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// OTA Configuration  
const char* OTA_PASSWORD = "YOUR_OTA_PASSWORD";

// Pushover Configuration
const char* PUSHOVER_TOKEN = "YOUR_PUSHOVER_APP_TOKEN";
const char* PUSHOVER_USER = "YOUR_PUSHOVER_USER_KEY";

// MQTT Configuration
const char* MQTT_SERVER = "YOUR_MQTT_SERVER_IP";      // e.g., "192.168.1.100"
const int MQTT_PORT = 1883;                           // Default MQTT port (1883 or 8883 for SSL)
const char* MQTT_USER = "YOUR_MQTT_USERNAME";         // Can be "" if no auth required
const char* MQTT_PASSWORD = "YOUR_MQTT_PASSWORD";     // Can be "" if no auth required
