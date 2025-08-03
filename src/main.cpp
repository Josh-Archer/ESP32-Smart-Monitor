#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "config.h"
#include "telnet.h"
#include "notifications.h"
#include "dns_manager.h"
#include "ota_manager.h"

Preferences preferences;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.print("\r\n\033[2J\033[H");

  Serial.printf("[%10lu ms] === ESP32-C3 Booting ===\r\n", millis());
  Serial.printf("[%10lu ms] Firmware Version: %s\r\n", millis(), firmwareVersion);

  // Initialize preferences with error handling
  bool prefsOK = preferences.begin("firmware", false);
  if (!prefsOK) {
    Serial.printf("[%10lu ms] [ERROR] Failed to initialize preferences, clearing NVS...\r\n", millis());
    preferences.clear();
    preferences.end();
    delay(100);
    prefsOK = preferences.begin("firmware", false);
  }
  
  if (prefsOK) {
    String lastVersion = preferences.getString("lastVersion", "");
    String currentVersion = String(firmwareVersion);
    
    if (lastVersion != currentVersion) {
      if (lastVersion != "" && lastVersion.length() > 0) {
        Serial.printf("[%10lu ms] [OTA] FIRMWARE UPDATED! Previous: %s -> Current: %s\r\n", 
                      millis(), lastVersion.c_str(), currentVersion.c_str());
        
        preferences.putULong("updateTime", millis());
        preferences.putString("updateFrom", lastVersion);
      } else {
        Serial.printf("[%10lu ms] [BOOT] First boot with version tracking\r\n", millis());
      }
      
      preferences.putString("lastVersion", currentVersion);
      Serial.printf("[%10lu ms] [VERSION] Stored version: %s\r\n", millis(), currentVersion.c_str());
    } else {
      Serial.printf("[%10lu ms] [BOOT] Running known version: %s\r\n", millis(), currentVersion.c_str());
      
      unsigned long lastUpdateTime = preferences.getULong("updateTime", 0);
      String updateFrom = preferences.getString("updateFrom", "");
      if (lastUpdateTime > 0 && updateFrom != "") {
        Serial.printf("[%10lu ms] [INFO] Last OTA update was from %s at boot time %lu ms\r\n", 
                      millis(), updateFrom.c_str(), lastUpdateTime);
      }
    }
  } else {
    Serial.printf("[%10lu ms] [WARNING] Version tracking disabled - NVS error\r\n", millis());
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.printf("[%10lu ms] Connecting to WiFi: %s\r\n", millis(), ssid);

  // Wait for WiFi with timeout
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 60) {
    delay(500);
    Serial.print(".");
    wifiTimeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\r\n[%10lu ms] Connected! IP: %s\r\n", millis(), WiFi.localIP().toString().c_str());
    
    // Configure custom DNS servers
    WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), primaryDNS, fallbackDNS);
    Serial.printf("[%10lu ms] [DNS] Configured DNS - Primary: %s, Fallback: %s\r\n", 
                  millis(), primaryDNS.toString().c_str(), fallbackDNS.toString().c_str());
    
    // Test DNS resolution
    testDNSResolution();
    
  } else {
    Serial.printf("\r\n[%10lu ms] [ERROR] WiFi connection failed!\r\n", millis());
    return;
  }

  // Initialize modules
  initOTA();
  initTelnet();
}

void loop() {
  handleOTA();
  handleTelnet();
  
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    telnetPrintf("[%10lu ms] WiFi disconnected. Attempting reconnect...\r\n", now);
    WiFi.begin(ssid, password);
    delay(2000);
    return;
  }

  // Test DNS every 10 heartbeats (50 seconds)
  static int heartbeatCount = 0;
  if (heartbeatCount % 10 == 0) {
    testDNSResolution();
  }
  heartbeatCount++;

  telnetPrintf("[%10lu ms] [Heartbeat] Pinging %s...\r\n", now, apiEndpoint);

  WiFiClient client;
  HTTPClient http;
  http.begin(client, apiEndpoint);
  http.setTimeout(10000);
  
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    telnetPrintf("[%10lu ms] [Heartbeat] Response (%d): %s\r\n", millis(), httpCode, payload.c_str());
  } else {
    telnetPrintf("[%10lu ms] [Heartbeat] HTTP GET failed: %s\r\n", millis(), http.errorToString(httpCode).c_str());
    
    // If heartbeat fails, test DNS resolution
    if (httpCode == HTTPC_ERROR_CONNECTION_REFUSED || httpCode == -1) {
      telnetPrintf("[%10lu ms] [DEBUG] Heartbeat failed, testing DNS...\r\n", millis());
      testDNSResolution();
    }
  }

  http.end();
  delay(5000);
}