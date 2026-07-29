#include "config.h"
#include "credentials.h"
#include <cstdio>

const char* firmwareVersion = "2.9.0";

// WiFi Configuration (from credentials.h)
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Heartbeat / notification-api configuration
// Contract: device issues HTTP GET to the resolved endpoint; success = HTTP 200.
// notification-api (legacy): GET /heartbeat/{device_id}  e.g. /heartbeat/poop
// Generic health example: set heartbeatPath = "/health" (or any path your probe accepts).
// Base URL must not include a trailing slash.
const char* heartbeatBaseUrl = "http://notifications.archerfamily.io";
const char* heartbeatDeviceId = "poop";
// Leave empty to use /heartbeat/{heartbeatDeviceId}. Override for non-notification-api targets.
const char* heartbeatPath = "";

static char heartbeatEndpointBuf[256];
static bool heartbeatEndpointReady = false;

const char* getHeartbeatEndpoint() {
  if (!heartbeatEndpointReady) {
    if (heartbeatPath != nullptr && heartbeatPath[0] != '\0') {
      snprintf(heartbeatEndpointBuf, sizeof(heartbeatEndpointBuf), "%s%s",
               heartbeatBaseUrl, heartbeatPath);
    } else {
      snprintf(heartbeatEndpointBuf, sizeof(heartbeatEndpointBuf), "%s/heartbeat/%s",
               heartbeatBaseUrl, heartbeatDeviceId);
    }
    heartbeatEndpointReady = true;
  }
  return heartbeatEndpointBuf;
}

const char* otaPassword = OTA_PASSWORD;
const char* deviceName = "poop-monitor";

// DNS Configuration
// Use DISTINCT servers so fallback is real. Same IP for both makes failover a no-op.
// Recommended: primary = local recursive/filter (Pi-hole, AdGuard, etc.);
//              fallback = a second resolver (another local box or a public DNS).
IPAddress primaryDNS(192, 168, 68, 51);  // Local DNS example (e.g. Pi-hole)
IPAddress fallbackDNS(1, 1, 1, 1);      // Distinct fallback example (Cloudflare)

// Pushover Configuration (from credentials.h)
const char* pushoverToken = PUSHOVER_TOKEN;
const char* pushoverUser = PUSHOVER_USER;
const char* pushoverApiUrl = "https://api.pushover.net/1/messages.json";

// MQTT Configuration
const char* mqttServer = "homeassistant.local";      // Your MQTT broker IP
const int mqttPort = 1883;                     // MQTT port (1883 or 8883 for SSL)
const char* mqttUser = MQTT_USER;                     // MQTT username (empty if no auth)
const char* mqttPassword = MQTT_PASSWORD;                 // MQTT password (empty if no auth)
