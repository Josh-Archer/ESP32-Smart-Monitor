#ifndef DNS_MANAGER_H
#define DNS_MANAGER_H

#include <Arduino.h>

// Main DNS testing function - tests primary and fallback DNS with smart alerting
bool testDNSResolutionWithSmartAlerting();

// Legacy function name for backward compatibility
bool testDNSResolution();

// Helper functions for improved readability
bool testDNSServerConnectivity(const char* testUrl);
void handleSuccessfulDNSResolution();
void handlePrimaryDNSFailureWithFallback();
void handleCompleteDNSFailure();
bool shouldSendDNSDownAlert(unsigned long currentTime);
void sendDNSDownAlert(unsigned long downTime);
void resetDNSFailureTracking();

// Alert pause control functions
void pauseAlertsForMinutes(int minutes);
void pauseAlertsIndefinitely();
void resumeAlerts();
bool areAlertsPaused();
unsigned long getAlertsPausedTimeRemaining();

// Global alert pause state (for web interface access)
extern bool alertsPaused;
extern unsigned long alertsPausedUntil;

#endif
