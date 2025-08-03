#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <Preferences.h>

// Firmware version - increment this with each update
const char* firmwareVersion = "1.0.0";

const char* ssid = "Archer";
const char* password = "Sk1pp3r!";
const char* apiEndpoint = "http://notifications.archerfamily.io/heartbeat/poop";
const char* otaPassword = "josh1156"; // Change this to your preferred OTA password
const char* deviceName = "poop-monitor"; // Device name for OTA

Preferences preferences;

void setup() {
  Serial.begin(115200);
  delay(100); // Give Serial time to initialize
  Serial.print("\r\n\033[2J\033[H"); // Optional clear screen (ANSI)

  Serial.printf("[%10lu ms] === ESP32-C3 Booting ===\r\n", millis());
  Serial.printf("[%10lu ms] Firmware Version: %s\r\n", millis(), firmwareVersion);

  // Initialize preferences
  preferences.begin("firmware", false);
  
  // Check if this is a new version after OTA update
  String lastVersion = preferences.getString("lastVersion", "");
  String currentVersion = String(firmwareVersion);
  
  if (lastVersion != currentVersion) {
    if (lastVersion != "") {
      Serial.printf("[%10lu ms] [OTA] FIRMWARE UPDATED! Previous: %s -> Current: %s\r\n", 
                    millis(), lastVersion.c_str(), currentVersion.c_str());
      
      // Log the update time
      preferences.putULong("updateTime", millis());
      preferences.putString("updateFrom", lastVersion);
    } else {
      Serial.printf("[%10lu ms] [BOOT] First boot with version tracking\r\n", millis());
    }
    
    // Save current version
    preferences.putString("lastVersion", currentVersion);
    Serial.printf("[%10lu ms] [VERSION] Stored version: %s\r\n", millis(), currentVersion.c_str());
  } else {
    Serial.printf("[%10lu ms] [BOOT] Running known version: %s\r\n", millis(), currentVersion.c_str());
    
    // Show last update info if available
    unsigned long lastUpdateTime = preferences.getULong("updateTime", 0);
    String updateFrom = preferences.getString("updateFrom", "");
    if (lastUpdateTime > 0 && updateFrom != "") {
      Serial.printf("[%10lu ms] [INFO] Last OTA update was from %s at boot time %lu ms\r\n", 
                    millis(), updateFrom.c_str(), lastUpdateTime);
    }
  }

  WiFi.begin(ssid, password);
  Serial.printf("[%10lu ms] Connecting to WiFi: %s\r\n", millis(), ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.printf("\r\n[%10lu ms] Connected! IP: %s\r\n", millis(), WiFi.localIP().toString().c_str());

  // Initialize mDNS
  if (!MDNS.begin(deviceName)) {
    Serial.printf("[%10lu ms] Error setting up MDNS responder!\r\n", millis());
  } else {
    Serial.printf("[%10lu ms] mDNS responder started. Device: %s.local\r\n", millis(), deviceName);
  }

  // Configure OTA
  ArduinoOTA.setHostname(deviceName);
  ArduinoOTA.setPassword(otaPassword);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.printf("\r\n[%10lu ms] [OTA] *** UPDATE INITIATED ***\r\n", millis());
    Serial.printf("[%10lu ms] [OTA] Current version: %s\r\n", millis(), firmwareVersion);
    Serial.printf("[%10lu ms] [OTA] Updating %s...\r\n", millis(), type.c_str());
    Serial.printf("[%10lu ms] [OTA] WARNING: Do not power off device!\r\n", millis());
  });

  ArduinoOTA.onEnd([]() {
    Serial.printf("\r\n[%10lu ms] [OTA] *** UPDATE COMPLETED ***\r\n", millis());
    Serial.printf("[%10lu ms] [OTA] Device will restart in 3 seconds...\r\n", millis());
    delay(3000);
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static unsigned long lastReport = 0;
    unsigned long now = millis();
    
    // Report progress every 2 seconds to avoid spam
    if (now - lastReport > 2000) {
      unsigned int percent = (progress / (total / 100));
      Serial.printf("[%10lu ms] [OTA] Progress: %u%% (%u/%u bytes)\r\n", 
                    now, percent, progress, total);
      lastReport = now;
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("\r\n[%10lu ms] [OTA] *** UPDATE FAILED ***\r\n", millis());
    Serial.printf("[%10lu ms] [OTA] Error[%u]: ", millis(), error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Authentication Failed - Check password!");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed - Not enough space?");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed - Network issue?");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed - Transfer interrupted?");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed - Verification error?");
    }
    Serial.printf("[%10lu ms] [OTA] Device will continue with current firmware\r\n", millis());
  });

  ArduinoOTA.begin();
  Serial.printf("[%10lu ms] [OTA] Ready! IP: %s\r\n", millis(), WiFi.localIP().toString().c_str());
  Serial.printf("[%10lu ms] [OTA] Device: %s.local (Password: %s)\r\n", millis(), deviceName, otaPassword);
  Serial.printf("[%10lu ms] [OTA] Version: %s\r\n\r\n", millis(), firmwareVersion);
}

void loop() {
  // Handle OTA updates
  ArduinoOTA.handle();
  
  unsigned long now = millis(); // Time since boot in ms

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[%10lu ms] WiFi disconnected. Attempting reconnect...\r\n", now);
    WiFi.begin(ssid, password);
    delay(2000);
    return;
  }

  Serial.printf("[%10lu ms] [Heartbeat] Pinging %s...\r\n", now, apiEndpoint);

  HTTPClient http;
  http.begin(apiEndpoint);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.printf("[%10lu ms] [Heartbeat] Response: %s\r\n", millis(), payload.c_str());
  } else {
    Serial.printf("[%10lu ms] [Heartbeat] HTTP GET failed: %s\r\n", millis(), http.errorToString(httpCode).c_str());
  }

  http.end();

  delay(5000); // Wait 5 seconds
}
