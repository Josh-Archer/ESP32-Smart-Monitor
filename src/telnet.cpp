#include "telnet.h"
#include <time.h>
#include "config.h"
#include "web_server.h" // For addToTelnetLogBuffer
#include "mqtt_manager.h" // For publishTelnetLog
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
  
  // Get local time (timezone set by configTzTime in setup)
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char ts[20];
  strftime(ts, sizeof(ts), "%H:%M:%S", &timeinfo);
  // Prepend timestamp
  char finalBuf[600];
  snprintf(finalBuf, sizeof(finalBuf), "[%s] %s", ts, buffer);
  // Print to serial
  Serial.print(finalBuf);
  
  // Print to telnet client if connected
  if (telnetClient && telnetClient.connected()) {
    telnetClient.print(finalBuf);
  }
  
  // Add to web log buffer for streaming (remove trailing \r\n for cleaner web display)
  String logEntry;
  logEntry.reserve(600);
  logEntry = String(buffer);
  logEntry.trim(); // Remove trailing whitespace
  if (logEntry.length() > 0) {
    addToTelnetLogBuffer(logEntry);
    
    // Publish to MQTT for Home Assistant (with timestamp)
    String mqttLogEntry = String(ts) + " " + logEntry;
    publishTelnetLog(mqttLogEntry);
  }
}
