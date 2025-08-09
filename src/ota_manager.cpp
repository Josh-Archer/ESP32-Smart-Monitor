#include "ota_manager.h"
#include "config.h"
#include "telnet.h"
#include "notifications.h"
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <esp_ota_ops.h>

Preferences otaPrefs;

void initOTA() {
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
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.printf("\r\n[%10lu ms] [OTA] *** UPDATE INITIATED ***\r\n", millis());
    Serial.printf("[%10lu ms] [OTA] Current version: %s\r\n", millis(), firmwareVersion);
    Serial.printf("[%10lu ms] [OTA] Updating %s...\r\n", millis(), type.c_str());
  });

  ArduinoOTA.onEnd([]() {
    Serial.printf("\r\n[%10lu ms] [OTA] *** UPDATE COMPLETED ***\r\n", millis());
    Serial.printf("[%10lu ms] [OTA] Device will restart in 3 seconds...\r\n", millis());
    delay(3000);
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static unsigned long lastReport = 0;
    unsigned long now = millis();
    if (now - lastReport > 2000) {
      unsigned int percent = (progress / (total / 100));
      Serial.printf("[%10lu ms] [OTA] Progress: %u%% (%u/%u bytes)\r\n", now, percent, progress, total);
      lastReport = now;
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("\r\n[%10lu ms] [OTA] *** UPDATE FAILED ***\r\n", millis());
    Serial.printf("[%10lu ms] [OTA] Error[%u]: ", millis(), error);
    if (error == OTA_AUTH_ERROR) Serial.println("Authentication Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  Serial.printf("[%10lu ms] [OTA] Ready! Device: %s.local\r\n", millis(), deviceName);
  Serial.printf("[%10lu ms] [OTA] Version: %s\r\n\r\n", millis(), firmwareVersion);
}

void handleOTA() {
  ArduinoOTA.handle();
}

bool checkRollbackCondition() {
  otaPrefs.begin("ota_rollback", true);
  int bootFailCount = otaPrefs.getInt("boot_fail_count", 0);
  otaPrefs.end();
  
  return bootFailCount >= 10;
}

void markFirmwareValid() {
  // Mark current partition as valid to prevent rollback
  esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
  if (err == ESP_OK) {
    Serial.printf("[%10lu ms] [OTA] Firmware marked as valid\r\n", millis());
    telnetPrintf("[%10lu ms] [OTA] Firmware marked as valid\r\n", millis());
    
    // Reset boot failure counter on successful validation
    resetBootFailureCount();
  } else {
    Serial.printf("[%10lu ms] [OTA] Failed to mark firmware as valid: %s\r\n", millis(), esp_err_to_name(err));
    telnetPrintf("[%10lu ms] [OTA] Failed to mark firmware as valid: %s\r\n", millis(), esp_err_to_name(err));
  }
}

void handleOTARollback() {
  if (!checkRollbackCondition()) {
    return;
  }
  
  // Log rollback event
  Serial.printf("[%10lu ms] [OTA] *** FIRMWARE ROLLBACK TRIGGERED ***\r\n", millis());
  Serial.printf("[%10lu ms] [OTA] Boot failures exceeded threshold (10)\r\n", millis());
  Serial.printf("[%10lu ms] [OTA] Rolling back to previous firmware version\r\n", millis());
  
  telnetPrintf("[%10lu ms] [OTA] *** FIRMWARE ROLLBACK TRIGGERED ***\r\n", millis());
  telnetPrintf("[%10lu ms] [OTA] Boot failures exceeded threshold (10)\r\n", millis());
  telnetPrintf("[%10lu ms] [OTA] Rolling back to previous firmware version\r\n", millis());
  
  // Send Pushover alert about rollback
  char alertTitle[100];
  char alertMessage[200];
  snprintf(alertTitle, sizeof(alertTitle), "OTA Rollback - %s", deviceName);
  snprintf(alertMessage, sizeof(alertMessage), 
           "Device experienced 10+ boot failures. Rolling back from firmware v%s to previous version. Device will restart.",
           firmwareVersion);
  
  sendPushoverAlert(alertTitle, alertMessage, 1); // High priority alert
  
  // Clear boot failure count before rollback
  resetBootFailureCount();
  
  // Save rollback event to preferences for post-rollback logging
  otaPrefs.begin("ota_rollback", false);
  otaPrefs.putString("last_rollback_from", firmwareVersion);
  otaPrefs.putULong("rollback_time", millis());
  otaPrefs.end();
  
  delay(2000); // Give time for alert to send
  
  // Trigger ESP32 OTA rollback
  esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
  if (err != ESP_OK) {
    Serial.printf("[%10lu ms] [OTA] Rollback failed: %s\r\n", millis(), esp_err_to_name(err));
    telnetPrintf("[%10lu ms] [OTA] Rollback failed: %s\r\n", millis(), esp_err_to_name(err));
  }
  
  // If we reach here, rollback failed - reboot anyway to try recovery
  Serial.printf("[%10lu ms] [OTA] Manual reboot after rollback failure\r\n", millis());
  telnetPrintf("[%10lu ms] [OTA] Manual reboot after rollback failure\r\n", millis());
  delay(1000);
  ESP.restart();
}

int getBootFailureCount() {
  otaPrefs.begin("ota_rollback", true);
  int count = otaPrefs.getInt("boot_fail_count", 0);
  otaPrefs.end();
  return count;
}

void resetBootFailureCount() {
  otaPrefs.begin("ota_rollback", false);
  otaPrefs.putInt("boot_fail_count", 0);
  otaPrefs.end();
  
  Serial.printf("[%10lu ms] [OTA] Boot failure counter reset\r\n", millis());
}
