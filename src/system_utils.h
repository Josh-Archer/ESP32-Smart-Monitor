#ifndef SYSTEM_UTILS_H
#define SYSTEM_UTILS_H

#include <Arduino.h>

// Reboot the ESP32 with optional delay and message
void rebootDevice(unsigned long delayMs = 3000, const char* reason = "Manual reboot");

// Check if a reboot was requested via flag in NVS (Preferences)
bool checkRebootFlag();

// Set a reboot flag (for remote reboot requests)
void setRebootFlag(const char* reason = "Remote reboot request");

// Format uptime milliseconds to human readable string (e.g., "1h 23m 45s")
String formatUptime(unsigned long uptimeMs);

#endif
