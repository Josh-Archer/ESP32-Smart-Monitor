#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>

// Initialize OTA functionality
void initOTA();

// Handle OTA updates (call in loop)
void handleOTA();

// Check if firmware should rollback due to boot failures
bool checkRollbackCondition();

// Mark current firmware as valid (call after successful boot)
void markFirmwareValid();

// Handle rollback if needed (call during boot)
void handleOTARollback();

// Get current boot failure count
int getBootFailureCount();

// Reset boot failure counter
void resetBootFailureCount();

#endif
