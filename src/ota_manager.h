#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>

// Initialize OTA functionality
void initOTA();

// Handle OTA updates (call in loop)
void handleOTA();

#endif
