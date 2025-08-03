#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// Initialize web server for remote commands
void initWebServer();

// Handle web server requests
void handleWebServer();

#endif
