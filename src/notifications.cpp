#include "notifications.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

void sendPushoverAlert(const char* title, const char* message, int priority) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[%10lu ms] [PUSHOVER] Cannot send alert - no WiFi\r\n", millis());
    return;
  }

  WiFiClientSecure client;
  client.setInsecure(); // For simplicity - in production, use proper certificates
  HTTPClient http;
  
  if (http.begin(client, pushoverApiUrl)) {
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
    String postData = "token=" + String(pushoverToken) +
                     "&user=" + String(pushoverUser) +
                     "&title=" + String(title) +
                     "&message=" + String(message) +
                     "&priority=" + String(priority) +
                     "&device=" + String(deviceName);
    
    Serial.printf("[%10lu ms] [PUSHOVER] Sending alert: %s\r\n", millis(), title);
    
    int httpCode = http.POST(postData);
    
    if (httpCode > 0) {
      String response = http.getString();
      Serial.printf("[%10lu ms] [PUSHOVER] Alert sent successfully (%d): %s\r\n", millis(), httpCode, response.c_str());
    } else {
      Serial.printf("[%10lu ms] [PUSHOVER] Failed to send alert: %s\r\n", millis(), http.errorToString(httpCode).c_str());
    }
    
    http.end();
  } else {
    Serial.printf("[%10lu ms] [PUSHOVER] Failed to connect to Pushover API\r\n", millis());
  }
}
