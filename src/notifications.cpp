#include "notifications.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

String urlEncode(const char* str) {
  if (!str) {
    return "";
  }
  String encoded;
  encoded.reserve(strlen(str) * 3 / 2);
  const char* p = str;
  while (*p) {
    char c = *p++;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += '+';
    } else {
      char hex[4];
      snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
      encoded += hex;
    }
  }
  return encoded;
}

void sendPushoverAlert(const char* title, const char* message, int priority) {
  if (!pushoverToken || !*pushoverToken || !pushoverUser || !*pushoverUser) {
    Serial.printf("[%10lu ms] [PUSHOVER] Pushover disabled - token or user not configured\r\n", millis());
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[%10lu ms] [PUSHOVER] Cannot send alert - no WiFi\r\n", millis());
    return;
  }

  WiFiClientSecure client;
  client.setInsecure(); // For simplicity - in production, use proper certificates
  client.setHandshakeTimeout(3);
  client.setTimeout(3);

  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(3000);

  if (http.begin(client, pushoverApiUrl)) {
    http.setConnectTimeout(3000);
    http.setTimeout(3000);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "token=" + urlEncode(pushoverToken) +
                     "&user=" + urlEncode(pushoverUser) +
                     "&title=" + urlEncode(title) +
                     "&message=" + urlEncode(message) +
                     "&priority=" + String(priority) +
                     "&device=" + urlEncode(deviceName);

    Serial.printf("[%10lu ms] [PUSHOVER] Sending alert: %s\r\n", millis(), title ? title : "");

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

