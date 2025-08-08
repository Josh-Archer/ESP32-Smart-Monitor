#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// Initialize web server for remote commands
void initWebServer();

// Handle web server requests
void handleWebServer();

// Handler functions for web interface
void handleRoot();
void handleReboot();
void handleStatus();
void handleAlertPause();
void handleAlertResume();
void handleNotFound();

// Telnet streaming handlers
void handleTelnetStart();
void handleTelnetStop();
void handleTelnetOutput();

// Utility functions
String escapeJsonString(const String& input);
void addToTelnetLogBuffer(const String& logEntry);

#endif
