// =============================================================================
// Credentials template (secrets) — DO NOT put real secrets in git
// =============================================================================
//
// Setup:
//   1. Copy this file to:  src/credentials_private.h
//   2. Replace every YOUR_* placeholder with your real values
//   3. Build / flash as usual (pio run, upload_and_monitor.sh, etc.)
//
// src/credentials_private.h is gitignored. Non-secret settings (device name,
// MQTT broker host, DNS, API URLs, feature flags) stay in src/config.cpp.
//
// CI copies this template automatically so firmware still compiles without
// real secrets.
// =============================================================================

#pragma once

// WiFi
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

// OTA password (must match upload_flags --auth in platformio.ini for OTA)
#define OTA_PASSWORD    "YOUR_OTA_PASSWORD"

// Pushover (https://pushover.net)
#define PUSHOVER_TOKEN  "YOUR_PUSHOVER_APP_TOKEN"
#define PUSHOVER_USER   "YOUR_PUSHOVER_USER_KEY"

// MQTT auth (use "" for both if the broker allows anonymous)
#define MQTT_USER       "YOUR_MQTT_USERNAME"
#define MQTT_PASSWORD   "YOUR_MQTT_PASSWORD"
