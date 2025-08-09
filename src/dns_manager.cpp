#include "dns_manager.h"
#include "config.h"
#include "notifications.h"
#include <WiFi.h>
#include <HTTPClient.h>

// Global variables for DNS failure tracking
bool dnsFailureReported = false;           // Prevent spam notifications
unsigned long dnsFirstFailureTime = 0;     // Track when DNS first failed
unsigned long lastDnsAlertTime = 0;        // Track when we last sent an alert
bool alertsPaused = false;                  // Manual alert pause control
unsigned long alertsPausedUntil = 0;       // Timestamp when paused alerts expire
unsigned long dnsRecoveryTime = 0;         // Track when DNS recovery started

// Global DNS status variables (defined here, declared extern in dns_manager.h)
bool isDNSWorking = true;
unsigned long lastDNSCheck = 0;
unsigned long dnsFailureStartTime = 0;

// Constants for DNS alert timing (5 minutes and 30 minutes)
const unsigned long DNS_FAILURE_THRESHOLD_MS = 5 * 60 * 1000;   // 5 minutes before first alert
const unsigned long DNS_ALERT_INTERVAL_MS = 30 * 60 * 1000;     // 30 minutes between alerts
const unsigned long DNS_RECOVERY_THRESHOLD_MS = 5 * 60 * 1000;  // 5 minutes before recovery alert

// Test DNS server connectivity using a reliable external service
bool testDNSServerConnectivity(const char* testUrl) {
  WiFiClient client;
  HTTPClient http;
  
  http.begin(client, testUrl);
  http.setTimeout(3000); // shorter timeout to reduce blocking
  
  int httpResponseCode = http.GET();
  http.end();
  
  return (httpResponseCode > 0);
}

// Handle successful DNS resolution - send recovery notification if needed
void handleSuccessfulDNSResolution() {
  Serial.printf("[%10lu ms] [DNS] DNS resolution working (Primary: %s)\r\n", millis(), primaryDNS.toString().c_str());
  isDNSWorking = true;
  lastDNSCheck = millis();
  dnsFailureStartTime = 0;
  
  if (dnsFailureReported) {
    unsigned long currentTime = millis();

    // Start tracking recovery time if not already tracking
    if (dnsRecoveryTime == 0) {
      dnsRecoveryTime = currentTime;
      Serial.printf("[%10lu ms] [DNS] Started tracking DNS recovery\r\n", currentTime);
    }

    // Check if DNS has been stable for the threshold time before sending recovery alert
    unsigned long timeSinceRecovery = currentTime - dnsRecoveryTime;
    if (timeSinceRecovery >= DNS_RECOVERY_THRESHOLD_MS) {
      // DNS has been stable for 5+ minutes, send recovery notification ONCE per instability event
      if (!areAlertsPaused()) {
        String recoveryMessage = "DNS server " + primaryDNS.toString() + " has been stable for " + 
                                String(timeSinceRecovery / 60000) + " minutes on " + String(deviceName);
        sendPushoverAlert("DNS Recovered", recoveryMessage.c_str(), 0);
        Serial.printf("[%10lu ms] [DNS] Recovery alert sent - DNS stable for %lu minutes\r\n", 
                      currentTime, timeSinceRecovery / 60000);
        // After sending, reset tracking so we don't send again until next instability
        resetDNSFailureTracking();
        // Auto-resume alerts on confirmed recovery
        if (areAlertsPaused()) {
          resumeAlerts();
          Serial.printf("[%10lu ms] [DNS] Auto-resumed alerts due to confirmed DNS recovery\r\n", currentTime);
        }
      } else {
        Serial.printf("[%10lu ms] [DNS] Recovery alert suppressed - alerts are paused\r\n", currentTime);
        // Still reset tracking so we don't send again until next instability
        resetDNSFailureTracking();
        if (areAlertsPaused()) {
          resumeAlerts();
          Serial.printf("[%10lu ms] [DNS] Auto-resumed alerts due to confirmed DNS recovery\r\n", currentTime);
        }
      }
    } else {
      // DNS is working but hasn't been stable long enough yet
      unsigned long minutesUntilAlert = (DNS_RECOVERY_THRESHOLD_MS - timeSinceRecovery) / 60000;
      Serial.printf("[%10lu ms] [DNS] DNS working for %lu minutes, recovery alert in %lu minutes\r\n", 
                    currentTime, timeSinceRecovery / 60000, minutesUntilAlert);
    }
  } else {
    // DNS was never reported as failed, reset recovery tracking
    dnsRecoveryTime = 0;
  }
}

// Reset all DNS failure tracking variables
void resetDNSFailureTracking() {
  dnsFailureReported = false;
  dnsFirstFailureTime = 0;
  lastDnsAlertTime = 0;
  dnsRecoveryTime = 0;
}

// Alert pause control functions
void pauseAlertsForMinutes(int minutes) {
  alertsPaused = true;
  alertsPausedUntil = millis() + (minutes * 60 * 1000);
  Serial.printf("[%10lu ms] [DNS] Alerts paused for %d minutes\r\n", millis(), minutes);
}

void pauseAlertsIndefinitely() {
  alertsPaused = true;
  alertsPausedUntil = 0;  // 0 means indefinite pause
  Serial.printf("[%10lu ms] [DNS] Alerts paused indefinitely\r\n", millis());
}

void resumeAlerts() {
  alertsPaused = false;
  alertsPausedUntil = 0;
  Serial.printf("[%10lu ms] [DNS] Alerts resumed\r\n", millis());
}

bool areAlertsPaused() {
  if (!alertsPaused) {
    return false;
  }
  
  // Check if timed pause has expired
  if (alertsPausedUntil > 0 && millis() >= alertsPausedUntil) {
    resumeAlerts();
    return false;
  }
  
  return true;
}

unsigned long getAlertsPausedTimeRemaining() {
  if (!alertsPaused || alertsPausedUntil == 0) {
    return 0;
  }
  
  unsigned long currentTime = millis();
  if (currentTime >= alertsPausedUntil) {
    return 0;
  }
  
  return (alertsPausedUntil - currentTime) / 1000; // Return seconds remaining
}

// Check if we should send a DNS down alert based on timing rules
bool shouldSendDNSDownAlert(unsigned long currentTime) {
  if (dnsFirstFailureTime == 0) {
    return false; // No failure tracked yet
  }
  
  unsigned long timeSinceFirstFailure = currentTime - dnsFirstFailureTime;
  
  // Must be down for at least 5 minutes before first alert
  if (timeSinceFirstFailure < DNS_FAILURE_THRESHOLD_MS) {
    return false;
  }
  
  // First alert after 5 minutes
  if (lastDnsAlertTime == 0) {
    return true;
  }
  
  // Subsequent alerts every 30 minutes
  unsigned long timeSinceLastAlert = currentTime - lastDnsAlertTime;
  return (timeSinceLastAlert >= DNS_ALERT_INTERVAL_MS);
}

// Send DNS down alert with timing information
void sendDNSDownAlert(unsigned long downTimeMs) {
  // Check if alerts are paused
  if (areAlertsPaused()) {
    unsigned long timeRemaining = getAlertsPausedTimeRemaining();
    if (timeRemaining > 0) {
      Serial.printf("[%10lu ms] [DNS] Alert suppressed - paused for %lu more seconds\r\n", 
                    millis(), timeRemaining);
    } else {
      Serial.printf("[%10lu ms] [DNS] Alert suppressed - paused indefinitely\r\n", millis());
    }
    lastDnsAlertTime = millis(); // Update timing to prevent immediate alert when resumed
    return;
  }
  
  unsigned long downTimeMinutes = downTimeMs / 60000;
  String alertMessage = "Primary DNS " + primaryDNS.toString() + " has been down for " + 
                       String(downTimeMinutes) + " minutes on " + String(deviceName) + 
                       ". Using fallback DNS.";
  
  sendPushoverAlert("DNS Server Down", alertMessage.c_str(), 1);
  lastDnsAlertTime = millis();
  
  Serial.printf("[%10lu ms] [DNS] Alert sent - primary DNS down for %lu minutes\r\n", 
                millis(), downTimeMinutes);
}

// Handle primary DNS failure when fallback DNS is working
void handlePrimaryDNSFailureWithFallback() {
  Serial.printf("[%10lu ms] [DNS] Fallback DNS working\r\n", millis());
  // Primary failed but overall DNS is assumed working via fallback
  isDNSWorking = true;
  lastDNSCheck = millis();
  
  unsigned long currentTime = millis();
  
  // Start tracking failure time if not already tracking
  if (dnsFirstFailureTime == 0) {
    dnsFirstFailureTime = currentTime;
    Serial.printf("[%10lu ms] [DNS] Started tracking primary DNS failure\r\n", currentTime);
  }
  
  // Check if we should send an alert
  if (shouldSendDNSDownAlert(currentTime)) {
    unsigned long timeSinceFirstFailure = currentTime - dnsFirstFailureTime;
    sendDNSDownAlert(timeSinceFirstFailure);
  } else {
    unsigned long timeSinceFirstFailure = currentTime - dnsFirstFailureTime;
    unsigned long downTimeMinutes = timeSinceFirstFailure / 60000;
    Serial.printf("[%10lu ms] [DNS] Primary DNS down for %lu minutes, not alerting yet\r\n", 
                  currentTime, downTimeMinutes);
  }
}

// Handle complete DNS failure (both primary and fallback failed)
void handleCompleteDNSFailure() {
  Serial.printf("[%10lu ms] [DNS] Both primary and fallback DNS failed!\r\n", millis());
  // Mark overall DNS as down
  isDNSWorking = false;
  lastDNSCheck = millis();
  if (dnsFailureStartTime == 0) {
    dnsFailureStartTime = lastDNSCheck;
  }
  
  // Only send critical alert if primary and fallback DNS are actually different servers
  // If they're the same, we're just testing the same server twice
  if (primaryDNS != fallbackDNS) {
    // For complete DNS failure with different servers, alert immediately (this is critical)
    if (!dnsFailureReported) {
      String criticalMessage = "Both primary (" + primaryDNS.toString() + 
                              ") and fallback (" + fallbackDNS.toString() + 
                              ") DNS failed on " + String(deviceName);
      sendPushoverAlert("Critical: All DNS Down", criticalMessage.c_str(), 2);
      dnsFailureReported = true;
    }
  } else {
    Serial.printf("[%10lu ms] [DNS] Primary and fallback DNS are the same (%s), skipping critical alert\r\n", 
                  millis(), primaryDNS.toString().c_str());
    // Still track this as a failure but don't send critical alert since it's the same server
    if (!dnsFailureReported) {
      dnsFailureReported = true;  // Prevent repeated attempts
    }
  }
}

// Main DNS testing function with smart alerting logic
bool testDNSResolutionWithSmartAlerting() {
  Serial.printf("[%10lu ms] [DNS] Testing DNS resolution...\r\n", millis());
  
  // Test with a reliable external service
  const char* testUrl = "http://httpbin.org/ip";
  bool primaryDNSWorking = testDNSServerConnectivity(testUrl);
  
  if (primaryDNSWorking) {
    handleSuccessfulDNSResolution();
    return true;
  }
  
  // Primary DNS failed, check if fallback is different before testing
  Serial.printf("[%10lu ms] [DNS] DNS resolution failed with primary DNS (%s)\r\n", 
                millis(), primaryDNS.toString().c_str());
  
  // Avoid reconfiguring DNS servers at runtime to prevent churn/instability
  if (primaryDNS == fallbackDNS) {
    Serial.printf("[%10lu ms] [DNS] Primary and fallback DNS are identical (%s), treating as complete failure\r\n", 
                  millis(), primaryDNS.toString().c_str());
    handleCompleteDNSFailure();
    return false;
  }

  // With distinct fallback configured, assume overall DNS remains operational via fallback
  handlePrimaryDNSFailureWithFallback();
  return true;
}

// Legacy function name for backward compatibility
bool testDNSResolution() {
  return testDNSResolutionWithSmartAlerting();
}
