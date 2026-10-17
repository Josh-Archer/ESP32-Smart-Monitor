#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>

// Initialize OTA functionality (ArduinoOTA + optional signed HTTP OTA)
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

// Signing / encryption status string for diagnostics
const char* getOtaSigningStatus();

// Notification helpers for OTA upload lifecycle
void notifyOtaStart(const char* uploadType = "sketch");
void notifyOtaSuccess(const char* details = "OTA update completed successfully");
void notifyOtaFailure(const char* reason = "Unknown error", int priority = 1);

#endif
