#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "Archer";
const char* password = "Sk1pp3r!";
const char* apiEndpoint = "http://notifications.archerfamily.io/heartbeat/poop";

void setup() {
  Serial.begin(115200);
  delay(100); // Give Serial time to initialize
  Serial.print("\r\n\033[2J\033[H"); // Optional clear screen (ANSI)

  Serial.printf("[%10lu ms] === ESP32-C3 Booting ===\r\n", millis());

  WiFi.begin(ssid, password);
  Serial.printf("[%10lu ms] Connecting to WiFi: %s\r\n", millis(), ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.printf("\r\n[%10lu ms] Connected! IP: %s\r\n\r\n", millis(), WiFi.localIP().toString().c_str());
}

void loop() {
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

