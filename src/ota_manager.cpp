#include "ota_manager.h"
#include "config.h"
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

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
