#include "telnet.h"
#include "config.h"
#include <stdarg.h>

// Telnet server for remote serial monitoring
WiFiServer telnetServer(23);
WiFiClient telnetClient;

void initTelnet() {
  telnetServer.begin();
  Serial.printf("[%10lu ms] [TELNET] Server started on port 23\r\n", millis());
  Serial.printf("[%10lu ms] [TELNET] Connect via: telnet %s 23\r\n", millis(), WiFi.localIP().toString().c_str());
}

void handleTelnet() {
  // Handle telnet connections
  if (telnetServer.hasClient()) {
    if (telnetClient) telnetClient.stop();
    telnetClient = telnetServer.available();
    Serial.printf("[%10lu ms] [TELNET] Client connected from %s\r\n", millis(), telnetClient.remoteIP().toString().c_str());
    telnetClient.println("=== ESP32 Telnet Console ===");
    telnetClient.printf("Device: %s | Version: %s\r\n", deviceName, firmwareVersion);
    telnetClient.printf("IP: %s | Uptime: %lu ms\r\n", WiFi.localIP().toString().c_str(), millis());
    telnetClient.println("============================");
  }
}

void telnetPrintf(const char* format, ...) {
  char buffer[512];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  // Print to serial
  Serial.print(buffer);
  
  // Print to telnet client if connected
  if (telnetClient && telnetClient.connected()) {
    telnetClient.print(buffer);
  }
}
