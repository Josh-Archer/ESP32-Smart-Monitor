#include <Arduino.h>
#include <time.h>
#include <sys/time.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include "config.h"
#include "telnet.h"
#include "notifications.h"
#include "dns_manager.h"
#include "ota_manager.h"
#include "system_utils.h"

#ifdef ENABLE_WEBSERVER
#include "web_server.h"
#endif

#ifdef ENABLE_MQTT
#include "mqtt_manager.h"
#endif

Preferences preferences;

// Global variables for tracking heartbeat status
unsigned long lastSuccessfulHeartbeat = 0;
int lastHeartbeatResponseCode = 0;

// Track time of last firmware update to avoid sleep for 30 minutes
unsigned long firmwareUpdateTime = 0;

// Power management constants
const unsigned long STATUS_INTERVAL_MS = 5 * 60 * 1000;      // 5 minutes
const uint64_t STATUS_INTERVAL_US = STATUS_INTERVAL_MS * 1000ULL;
const unsigned long NO_SLEEP_AFTER_UPDATE_MS = 30 * 60 * 1000; // 30 minutes
const int OTA_WINDOW_MINUTES = 15;                             // First 15 min each hour

bool shouldStayAwake() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  bool otaWindow = timeinfo.tm_min < OTA_WINDOW_MINUTES;
  bool recentUpdate = (firmwareUpdateTime > 0) &&
                      ((millis() - firmwareUpdateTime) < NO_SLEEP_AFTER_UPDATE_MS);
  return otaWindow || recentUpdate;
}

void enterDeepSleep() {
#ifdef ENABLE_MQTT
  publishAvailability(false);
  delay(100);
#endif
  esp_sleep_enable_timer_wakeup(STATUS_INTERVAL_US);
  Serial.printf("[%10lu ms] [POWER] Entering deep sleep for 5 minutes\r\n", millis());
  esp_deep_sleep_start();
}

void enterLightSleepUntilDNSRestored() {
  sendPushoverAlert("DNS Down", "Entering light sleep until DNS recovers", 1);
  while (!testDNSResolution()) {
    Serial.printf("[%10lu ms] [POWER] DNS down - light sleeping 60s\r\n", millis());
    esp_sleep_enable_timer_wakeup(60ULL * 1000000ULL);
    esp_light_sleep_start();
  }
  Serial.printf("[%10lu ms] [POWER] DNS restored - resuming normal operation\r\n", millis());
}

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
  
  // Check for rollback conditions before proceeding
  if (checkRollbackCondition()) {
    Serial.printf("[%10lu ms] [OTA] Boot failure threshold exceeded - triggering rollback\r\n", millis());
    handleOTARollback();
    // Will not return if rollback succeeds
  }
  
  // Increment boot failure counter at start of boot
  Preferences bootPrefs;
  bootPrefs.begin("ota_rollback", false);
  int bootFailCount = bootPrefs.getInt("boot_fail_count", 0) + 1;
  bootPrefs.putInt("boot_fail_count", bootFailCount);
  bootPrefs.end();
  
  Serial.printf("[%10lu ms] [OTA] Boot attempt #%d\r\n", millis(), bootFailCount);
  
  // Check if this boot followed a rollback
  bootPrefs.begin("ota_rollback", true);
  String lastRollbackFrom = bootPrefs.getString("last_rollback_from", "");
  unsigned long rollbackTime = bootPrefs.getULong("rollback_time", 0);
  bootPrefs.end();
  
  if (lastRollbackFrom != "" && rollbackTime > 0) {
    Serial.printf("[%10lu ms] [OTA] *** ROLLBACK RECOVERY DETECTED ***\r\n", millis());
    Serial.printf("[%10lu ms] [OTA] Rolled back from version: %s\r\n", millis(), lastRollbackFrom.c_str());
    Serial.printf("[%10lu ms] [OTA] Current version: %s\r\n", millis(), firmwareVersion);
    
    // Clear rollback tracking since we've detected it
    bootPrefs.begin("ota_rollback", false);
    bootPrefs.remove("last_rollback_from");
    bootPrefs.remove("rollback_time");
    bootPrefs.end();
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
        firmwareUpdateTime = millis();
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
    
    // Configure time for OTA scheduling
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    time_t now = time(nullptr);
    while (now < 100000) {
      delay(500);
      now = time(nullptr);
    }

    // Test DNS resolution
    testDNSResolution();
    
  } else {
    Serial.printf("\r\n[%10lu ms] [ERROR] WiFi connection failed!\r\n", millis());
    return;
  }

  // Initialize modules
  initOTA();
  initTelnet();

#ifdef ENABLE_WEBSERVER
  initWebServer();
#endif

#ifdef ENABLE_MQTT
  initializeMQTT();  // Initialize MQTT for Home Assistant integration
#endif
  
  // Check if a reboot was requested before last boot
  if (checkRebootFlag()) {
    telnetPrintf("[%10lu ms] [SYSTEM] Device rebooted successfully\r\n", millis());
  }
  
  // Mark firmware as valid after successful module initialization
  // This will reset the boot failure counter and prevent rollback
  markFirmwareValid();
  
  Serial.printf("[%10lu ms] [BOOT] Setup completed successfully\r\n", millis());
  telnetPrintf("[%10lu ms] [BOOT] Setup completed successfully for v%s\r\n", millis(), firmwareVersion);
}

void loop() {
  handleOTA();
  handleTelnet();

#ifdef ENABLE_WEBSERVER
  handleWebServer();
#endif

#ifdef ENABLE_MQTT
  handleMQTTLoop();
#endif

  // Check for reboot flag (set by web interface)
  if (checkRebootFlag()) {
#ifdef ENABLE_MQTT
    publishAvailability(false);
    delay(100);
#endif
    rebootDevice(3000, "Remote reboot request");
  }

  if (WiFi.status() != WL_CONNECTED) {
    telnetPrintf("[%10lu ms] WiFi disconnected. Attempting reconnect...\r\n", millis());
    WiFi.begin(ssid, password);
    delay(2000);
    return;
  }

  // Verify DNS and enter light sleep if down
  if (!testDNSResolution()) {
    enterLightSleepUntilDNSRestored();
  }

  WiFiClient client;
  HTTPClient http;
  http.begin(client, apiEndpoint);
  http.setTimeout(10000);

  int httpCode = http.GET();
  lastHeartbeatResponseCode = httpCode;

  if (httpCode > 0) {
    String payload = http.getString();
    telnetPrintf("[%10lu ms] [Heartbeat] Ping Response (%d): %s\r\n", millis(), httpCode, payload.c_str());

    if (httpCode == 200) {
      lastSuccessfulHeartbeat = millis();
    }
  } else {
    telnetPrintf("[%10lu ms] [Heartbeat] Ping failed: %s\r\n", millis(), http.errorToString(httpCode).c_str());
  }

  http.end();

  if (shouldStayAwake()) {
    Serial.printf("[%10lu ms] [POWER] Staying awake for OTA or update window\r\n", millis());
    delay(STATUS_INTERVAL_MS);
  } else {
    enterDeepSleep();
  }
}