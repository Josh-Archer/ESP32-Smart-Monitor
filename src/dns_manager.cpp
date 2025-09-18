#include "dns_manager.h"
#include "config.h"
#include "notifications.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>

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

// Runtime-adjustable DNS timing (defaults set here; can be changed via MQTT command interface)
unsigned long dnsFailureThresholdMs = 5UL * 60UL * 1000UL;            // Down this long before first alert
unsigned long dnsAlertIntervalMs = 30UL * 60UL * 1000UL;              // Interval between repeated down alerts
unsigned long dnsRecoveryThresholdMs = 5UL * 60UL * 1000UL;           // Continuous healthy period before recovery alert
unsigned long dnsMinFailureDurationForRecoveryMs = 60UL * 1000UL;     // Minimum outage duration to qualify for recovery flow

// Immutable defaults for validation / reset
const unsigned long DNS_DEFAULT_FAILURE_THRESHOLD_MS = 5UL * 60UL * 1000UL;
const unsigned long DNS_DEFAULT_ALERT_INTERVAL_MS = 30UL * 60UL * 1000UL;
const unsigned long DNS_DEFAULT_RECOVERY_THRESHOLD_MS = 5UL * 60UL * 1000UL;
const unsigned long DNS_DEFAULT_MIN_FAILURE_FOR_RECOVERY_MS = 60UL * 1000UL;

static bool dnsConfigLoaded = false;

// Minimum failure duration rationale:
// Very short DNS hiccups (<60s) are treated as micro blips and will NOT trigger the
// recovery tracking / recovery notification path. This avoids noisy "Recovered" alerts
// for transient packet loss or momentary upstream resolver stalls. Adjust this threshold
// if your network environment exhibits longer benign hiccups.

// NOTE: Recovery debounce explanation
// We only send a "DNS Recovered" alert after DNS has been continuously healthy
// for dnsRecoveryThresholdMs without ANY complete failure in between.
// Partial failures (primary down but fallback working) do not reset the timer;
// complete failures (both primary & fallback) reset dnsRecoveryTime to enforce
// a contiguous healthy period and prevent alert flapping.

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

  // If prior failure was extremely brief (< dnsMinFailureDurationForRecoveryMs) treat it as a micro blip
    // and do NOT start recovery tracking / send a recovery alert. This prevents noisy up/down sequences
    // (e.g., transient packet loss) from generating meaningless "DNS Recovered" notifications.
    // We rely on dnsFailureReported flag for real outages; micro blips should clear that state.
    if (dnsFirstFailureTime != 0) {
      unsigned long failureDuration = currentTime - dnsFirstFailureTime;
  if (failureDuration < dnsMinFailureDurationForRecoveryMs) {
        Serial.printf("[%10lu ms] [DNS] Previous failure lasted %lu ms (< %lu ms min); suppressing recovery tracking & alert\r\n",
          currentTime, failureDuration, dnsMinFailureDurationForRecoveryMs);
        resetDNSFailureTracking();
        // Do not proceed with recovery timer logic for micro blip
        return;
      }
    }

    // Start tracking recovery time if not already tracking
    if (dnsRecoveryTime == 0) {
      dnsRecoveryTime = currentTime;
      Serial.printf("[%10lu ms] [DNS] Started tracking DNS recovery\r\n", currentTime);
    }

    // Check if DNS has been stable for the threshold time before sending recovery alert
    unsigned long timeSinceRecovery = currentTime - dnsRecoveryTime;
  if (timeSinceRecovery >= dnsRecoveryThresholdMs) {
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
  unsigned long minutesUntilAlert = (dnsRecoveryThresholdMs - timeSinceRecovery) / 60000;
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
  if (timeSinceFirstFailure < dnsFailureThresholdMs) {
    return false;
  }
  
  // First alert after 5 minutes
  if (lastDnsAlertTime == 0) {
    return true;
  }
  
  // Subsequent alerts every 30 minutes
  unsigned long timeSinceLastAlert = currentTime - lastDnsAlertTime;
  return (timeSinceLastAlert >= dnsAlertIntervalMs);
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
  // IMPORTANT: Any complete failure invalidates prior recovery stability tracking.
  // If dnsRecoveryTime was already counting (we had partial recovery but failed again
  // before reaching DNS_RECOVERY_THRESHOLD_MS), clear it so we require a *continuous*
  // healthy window before sending a "DNS Recovered" alert. Otherwise intermittent
  // flapping could accumulate non-contiguous uptime and spam recovery alerts.
  if (dnsRecoveryTime != 0) {
    Serial.printf("[%10lu ms] [DNS] Resetting recovery stability timer due to renewed failure (was tracking since +%lu ms)\r\n",
                  millis(), dnsRecoveryTime);
    dnsRecoveryTime = 0;
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

// Update DNS timing configuration (0 parameters mean 'leave unchanged')
void updateDNSConfig(unsigned long failureThresholdMs,
                     unsigned long alertIntervalMs,
                     unsigned long recoveryThresholdMs,
                     unsigned long minFailureForRecoveryMs) {
  bool changed = false;
  const unsigned long MAX_ALLOWED_MS = 24UL * 60UL * 60UL * 1000UL; // 24 hours
  auto validate = [&](unsigned long value, unsigned long def, unsigned long minReq) -> unsigned long {
    if (value == 0) return 0; // unchanged
    if (value < minReq || value > MAX_ALLOWED_MS) return def; // invalid -> default
    return value;
  };
  unsigned long vFailure = validate(failureThresholdMs, DNS_DEFAULT_FAILURE_THRESHOLD_MS, 30UL * 1000UL);
  unsigned long vInterval = validate(alertIntervalMs, DNS_DEFAULT_ALERT_INTERVAL_MS, 60UL * 1000UL);
  unsigned long vRecovery = validate(recoveryThresholdMs, DNS_DEFAULT_RECOVERY_THRESHOLD_MS, 30UL * 1000UL);
  unsigned long vMinFail = validate(minFailureForRecoveryMs, DNS_DEFAULT_MIN_FAILURE_FOR_RECOVERY_MS, 5UL * 1000UL);
  if (vFailure && vFailure != dnsFailureThresholdMs) { dnsFailureThresholdMs = vFailure; changed = true; }
  if (vInterval && vInterval != dnsAlertIntervalMs) { dnsAlertIntervalMs = vInterval; changed = true; }
  if (vRecovery && vRecovery != dnsRecoveryThresholdMs) { dnsRecoveryThresholdMs = vRecovery; changed = true; }
  if (vMinFail && vMinFail != dnsMinFailureDurationForRecoveryMs) { dnsMinFailureDurationForRecoveryMs = vMinFail; changed = true; }
  if (changed) {
    Serial.printf("[DNS] Updated config: failureThreshold=%lu ms, alertInterval=%lu ms, recoveryThreshold=%lu ms, minFailureForRecovery=%lu ms\r\n",
                  dnsFailureThresholdMs, dnsAlertIntervalMs, dnsRecoveryThresholdMs, dnsMinFailureDurationForRecoveryMs);
    // Persist
    Preferences prefs;
    if (prefs.begin("dns_cfg", false)) {
      prefs.putULong("fail_thr", dnsFailureThresholdMs);
      prefs.putULong("alert_int", dnsAlertIntervalMs);
      prefs.putULong("rec_thr", dnsRecoveryThresholdMs);
      prefs.putULong("min_rec", dnsMinFailureDurationForRecoveryMs);
      prefs.end();
      Serial.println("[DNS] Config saved to NVS");
    } else {
      Serial.println("[DNS] Failed to open NVS for saving config");
    }
  } else {
    Serial.println("[DNS] updateDNSConfig called but no values changed");
  }
}

void loadDNSConfigFromStorage() {
  if (dnsConfigLoaded) return;
  Preferences prefs;
  if (!prefs.begin("dns_cfg", true)) {
    Serial.println("[DNS] No stored DNS config (RO open failed), using defaults");
    dnsConfigLoaded = true;
    return;
  }
  unsigned long f = prefs.getULong("fail_thr", DNS_DEFAULT_FAILURE_THRESHOLD_MS);
  unsigned long i = prefs.getULong("alert_int", DNS_DEFAULT_ALERT_INTERVAL_MS);
  unsigned long r = prefs.getULong("rec_thr", DNS_DEFAULT_RECOVERY_THRESHOLD_MS);
  unsigned long m = prefs.getULong("min_rec", DNS_DEFAULT_MIN_FAILURE_FOR_RECOVERY_MS);
  prefs.end();
  if (f == 0 || i == 0 || r == 0 || m == 0) {
    Serial.println("[DNS] Stored DNS config invalid (zero), reverting to defaults");
    f = DNS_DEFAULT_FAILURE_THRESHOLD_MS;
    i = DNS_DEFAULT_ALERT_INTERVAL_MS;
    r = DNS_DEFAULT_RECOVERY_THRESHOLD_MS;
    m = DNS_DEFAULT_MIN_FAILURE_FOR_RECOVERY_MS;
  }
  dnsFailureThresholdMs = f;
  dnsAlertIntervalMs = i;
  dnsRecoveryThresholdMs = r;
  dnsMinFailureDurationForRecoveryMs = m;
  dnsConfigLoaded = true;
  Serial.printf("[DNS] Loaded config: failureThreshold=%lu ms, alertInterval=%lu ms, recoveryThreshold=%lu ms, minFailureForRecovery=%lu ms\r\n",
                dnsFailureThresholdMs, dnsAlertIntervalMs, dnsRecoveryThresholdMs, dnsMinFailureDurationForRecoveryMs);
}

void saveDNSConfigToStorage() {
  // Provided for explicit save if needed elsewhere (currently updateDNSConfig persists automatically)
  Preferences prefs; 
  if (!prefs.begin("dns_cfg", false)) {
    Serial.println("[DNS] Failed to open NVS for explicit save");
    return;
  }
  prefs.putULong("fail_thr", dnsFailureThresholdMs);
  prefs.putULong("alert_int", dnsAlertIntervalMs);
  prefs.putULong("rec_thr", dnsRecoveryThresholdMs);
  prefs.putULong("min_rec", dnsMinFailureDurationForRecoveryMs);
  prefs.end();
  Serial.println("[DNS] Explicit config save complete");
}
