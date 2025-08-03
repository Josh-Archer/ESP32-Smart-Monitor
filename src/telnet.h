#ifndef TELNET_H
#define TELNET_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiServer.h>

// Initialize telnet server
void initTelnet();

// Handle telnet connections
void handleTelnet();

// Custom print function that outputs to both Serial and Telnet
void telnetPrintf(const char* format, ...);

#endif
