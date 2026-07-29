#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

// Network indices for multi-SSID self-healing
#define WIFI_NET_NONE      (-1)
#define WIFI_NET_PRIMARY   0
#define WIFI_NET_SECONDARY 1

// Boot-time connect: primary first, then secondary if configured
bool initWiFi();

// Runtime reconnect, failover, and periodic recovery to primary
void handleWiFi();

// Connection helpers
bool isWiFiConnected();
bool isSecondaryWiFiConfigured();
bool isOnPrimaryNetwork();

// Active network reporting (for MQTT / web status)
const char* getActiveSSID();
int getActiveNetworkIndex();
const char* getActiveNetworkRole();  // "primary", "secondary", or "none"

// Re-apply custom DNS after (re)connect
void applyWiFiDNS();

#endif
