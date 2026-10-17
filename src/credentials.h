#pragma once

// =============================================================================
// Required Credentials Declarations
// =============================================================================
// If the build fails with:
//   error: #error "src/credentials.cpp is missing! Copy credentials.template.cpp..."
// or with undefined references to WIFI_SSID, OTA_PASSWORD, etc.:
//
// FIX: Copy credentials.template.cpp to src/credentials.cpp and set your secrets:
//   cp credentials.template.cpp src/credentials.cpp
//
// src/credentials.cpp is gitignored to protect sensitive credentials.
// Never ship a device with missing Wi-Fi or OTA credentials.
// =============================================================================

extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;
// Optional secondary network for multi-SSID self-healing (empty string to disable)
extern const char* WIFI_SSID_SECONDARY;
extern const char* WIFI_PASSWORD_SECONDARY;
extern const char* OTA_PASSWORD;
extern const char* PUSHOVER_TOKEN;
extern const char* PUSHOVER_USER;
extern const char* MQTT_USER;
extern const char* MQTT_PASSWORD;
