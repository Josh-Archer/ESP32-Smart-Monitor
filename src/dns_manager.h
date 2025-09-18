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

// Runtime-adjustable DNS timing variables (exposed for MQTT config/status)
extern unsigned long dnsFailureThresholdMs;          // Down duration before first alert
extern unsigned long dnsAlertIntervalMs;             // Interval between repeated down alerts
extern unsigned long dnsRecoveryThresholdMs;         // Continuous healthy time before recovery alert
extern unsigned long dnsMinFailureDurationForRecoveryMs; // Minimum outage length to qualify for recovery alert

// Update DNS timing configuration (pass 0 to leave a value unchanged)
void updateDNSConfig(unsigned long failureThresholdMs,
					 unsigned long alertIntervalMs,
					 unsigned long recoveryThresholdMs,
					 unsigned long minFailureForRecoveryMs);

// Defaults (immutable) for validation / reset
extern const unsigned long DNS_DEFAULT_FAILURE_THRESHOLD_MS;
extern const unsigned long DNS_DEFAULT_ALERT_INTERVAL_MS;
extern const unsigned long DNS_DEFAULT_RECOVERY_THRESHOLD_MS;
extern const unsigned long DNS_DEFAULT_MIN_FAILURE_FOR_RECOVERY_MS;

// Persistence helpers
void loadDNSConfigFromStorage();
void saveDNSConfigToStorage();

// Alert pause control functions
void pauseAlertsForMinutes(int minutes);
void pauseAlertsIndefinitely();
void resumeAlerts();
bool areAlertsPaused();
unsigned long getAlertsPausedTimeRemaining();


// Global DNS status variables (for MQTT and web integration)
extern bool isDNSWorking;
extern unsigned long lastDNSCheck;
extern unsigned long dnsFailureStartTime;
extern bool alertsPaused;
extern unsigned long alertsPausedUntil;

#endif
