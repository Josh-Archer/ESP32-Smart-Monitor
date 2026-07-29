#include "wifi_manager.h"
#include "config.h"
#include "telnet.h"
#include <WiFi.h>
#include <string.h>

// Active network tracking
static int activeNetworkIndex = WIFI_NET_NONE;
static unsigned long lastReconnectAttempt = 0;
static unsigned long lastPrimaryRecoveryAttempt = 0;

// Timing (ms)
static const unsigned long CONNECT_TIMEOUT_MS = 30000;            // per SSID at boot
static const unsigned long RECONNECT_TIMEOUT_MS = 12000;          // per SSID while reconnecting
static const unsigned long RECONNECT_INTERVAL_MS = 5000;          // min gap between reconnect cycles
static const unsigned long PRIMARY_RECOVERY_INTERVAL_MS = 300000; // try primary every 5 min when on secondary
static const unsigned long PRIMARY_RECOVERY_TIMEOUT_MS = 15000;   // time allowed for primary recovery

static bool isNetworkConfigured(int index) {
  if (index == WIFI_NET_PRIMARY) {
    return ssid != nullptr && ssid[0] != '\0';
  }
  if (index == WIFI_NET_SECONDARY) {
    return ssidSecondary != nullptr && ssidSecondary[0] != '\0';
  }
  return false;
}

static const char* networkSSID(int index) {
  if (index == WIFI_NET_PRIMARY) {
    return ssid != nullptr ? ssid : "";
  }
  if (index == WIFI_NET_SECONDARY) {
    return ssidSecondary != nullptr ? ssidSecondary : "";
  }
  return "";
}

static const char* networkPassword(int index) {
  if (index == WIFI_NET_PRIMARY) {
    return password != nullptr ? password : "";
  }
  if (index == WIFI_NET_SECONDARY) {
    return passwordSecondary != nullptr ? passwordSecondary : "";
  }
  return "";
}

static const char* networkRoleName(int index) {
  if (index == WIFI_NET_PRIMARY) return "primary";
  if (index == WIFI_NET_SECONDARY) return "secondary";
  return "none";
}

void applyWiFiDNS() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), primaryDNS, fallbackDNS);
  Serial.printf("[%10lu ms] [DNS] Configured DNS - Primary: %s, Fallback: %s\r\n",
                millis(), primaryDNS.toString().c_str(), fallbackDNS.toString().c_str());
}

static bool tryConnect(int index, unsigned long timeoutMs) {
  if (!isNetworkConfigured(index)) {
    return false;
  }

  const char* netSsid = networkSSID(index);
  const char* netPass = networkPassword(index);

  Serial.printf("[%10lu ms] [WiFi] Connecting to '%s' (%s)...\r\n",
                millis(), netSsid, networkRoleName(index));

  WiFi.disconnect(false);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(netSsid, netPass);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    activeNetworkIndex = index;
    Serial.printf("[%10lu ms] [WiFi] Connected to '%s' (%s) | IP: %s | RSSI: %d dBm\r\n",
                  millis(), netSsid, networkRoleName(index),
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    applyWiFiDNS();
    return true;
  }

  Serial.printf("[%10lu ms] [WiFi] Failed to connect to '%s' (%s)\r\n",
                millis(), netSsid, networkRoleName(index));
  return false;
}

bool initWiFi() {
  WiFi.mode(WIFI_STA);

  // Prefer primary
  if (tryConnect(WIFI_NET_PRIMARY, CONNECT_TIMEOUT_MS)) {
    return true;
  }

  // Fail over to secondary when configured
  if (isNetworkConfigured(WIFI_NET_SECONDARY)) {
    Serial.printf("[%10lu ms] [WiFi] Primary unavailable — trying secondary...\r\n", millis());
    if (tryConnect(WIFI_NET_SECONDARY, CONNECT_TIMEOUT_MS)) {
      return true;
    }
  }

  activeNetworkIndex = WIFI_NET_NONE;
  Serial.printf("[%10lu ms] [ERROR] WiFi connection failed (all networks)!\r\n", millis());
  return false;
}

bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

bool isSecondaryWiFiConfigured() {
  return isNetworkConfigured(WIFI_NET_SECONDARY);
}

bool isOnPrimaryNetwork() {
  return WiFi.status() == WL_CONNECTED && activeNetworkIndex == WIFI_NET_PRIMARY;
}

const char* getActiveSSID() {
  if (WiFi.status() != WL_CONNECTED) {
    return "";
  }
  if (activeNetworkIndex == WIFI_NET_PRIMARY || activeNetworkIndex == WIFI_NET_SECONDARY) {
    return networkSSID(activeNetworkIndex);
  }
  // Fallback to runtime SSID if tracking is stale
  static char ssidBuf[33];
  String current = WiFi.SSID();
  strncpy(ssidBuf, current.c_str(), sizeof(ssidBuf) - 1);
  ssidBuf[sizeof(ssidBuf) - 1] = '\0';
  return ssidBuf;
}

int getActiveNetworkIndex() {
  if (WiFi.status() != WL_CONNECTED) {
    return WIFI_NET_NONE;
  }
  return activeNetworkIndex;
}

const char* getActiveNetworkRole() {
  if (WiFi.status() != WL_CONNECTED) {
    return "none";
  }
  return networkRoleName(activeNetworkIndex);
}

static void syncActiveIndexFromSSID() {
  if (WiFi.status() != WL_CONNECTED) {
    activeNetworkIndex = WIFI_NET_NONE;
    return;
  }
  if (activeNetworkIndex >= 0) {
    return;
  }
  String current = WiFi.SSID();
  if (isNetworkConfigured(WIFI_NET_PRIMARY) && current == String(ssid)) {
    activeNetworkIndex = WIFI_NET_PRIMARY;
  } else if (isNetworkConfigured(WIFI_NET_SECONDARY) && current == String(ssidSecondary)) {
    activeNetworkIndex = WIFI_NET_SECONDARY;
  }
}

static void attemptReconnectOrFailover() {
  unsigned long now = millis();
  if (now - lastReconnectAttempt < RECONNECT_INTERVAL_MS) {
    return;
  }
  lastReconnectAttempt = now;

  int preferred = (activeNetworkIndex >= 0) ? activeNetworkIndex : WIFI_NET_PRIMARY;
  int alternate = (preferred == WIFI_NET_PRIMARY) ? WIFI_NET_SECONDARY : WIFI_NET_PRIMARY;

  telnetPrintf("[%10lu ms] [WiFi] Disconnected. Reconnect/failover starting (prefer %s)...\r\n",
               now, networkRoleName(preferred));

  if (tryConnect(preferred, RECONNECT_TIMEOUT_MS)) {
    telnetPrintf("[%10lu ms] [WiFi] Reconnected to %s ('%s')\r\n",
                 millis(), networkRoleName(activeNetworkIndex), getActiveSSID());
    return;
  }

  if (isNetworkConfigured(alternate) && tryConnect(alternate, RECONNECT_TIMEOUT_MS)) {
    telnetPrintf("[%10lu ms] [WiFi] Failover to %s ('%s')\r\n",
                 millis(), networkRoleName(activeNetworkIndex), getActiveSSID());
    // Allow primary recovery probe soon after landing on secondary
    if (activeNetworkIndex == WIFI_NET_SECONDARY) {
      lastPrimaryRecoveryAttempt = millis();
    }
    return;
  }

  activeNetworkIndex = WIFI_NET_NONE;
  telnetPrintf("[%10lu ms] [WiFi] All networks failed this cycle\r\n", millis());
}

static void attemptPrimaryRecovery() {
  if (!isNetworkConfigured(WIFI_NET_SECONDARY)) {
    return;
  }
  if (activeNetworkIndex != WIFI_NET_SECONDARY || WiFi.status() != WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();
  if (now - lastPrimaryRecoveryAttempt < PRIMARY_RECOVERY_INTERVAL_MS) {
    return;
  }
  lastPrimaryRecoveryAttempt = now;

  telnetPrintf("[%10lu ms] [WiFi] On secondary; probing primary for recovery...\r\n", millis());

  if (tryConnect(WIFI_NET_PRIMARY, PRIMARY_RECOVERY_TIMEOUT_MS)) {
    telnetPrintf("[%10lu ms] [WiFi] Recovered to primary ('%s')\r\n", millis(), getActiveSSID());
    return;
  }

  // Restore secondary if primary is still down
  telnetPrintf("[%10lu ms] [WiFi] Primary still unavailable; restoring secondary\r\n", millis());
  if (!tryConnect(WIFI_NET_SECONDARY, RECONNECT_TIMEOUT_MS)) {
    activeNetworkIndex = WIFI_NET_NONE;
    telnetPrintf("[%10lu ms] [WiFi] Failed to restore secondary after primary probe\r\n", millis());
  }
}

void handleWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    attemptReconnectOrFailover();
    return;
  }

  syncActiveIndexFromSSID();
  attemptPrimaryRecovery();
}
