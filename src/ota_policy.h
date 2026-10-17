#ifndef OTA_POLICY_H
#define OTA_POLICY_H

#include <string.h>

static const int OTA_AUTH_ALERT_PRIORITY = 0;

// Shared OTA policy used by ota_manager.cpp and the native host tests.
inline bool otaPasswordConfigured(const char* password) {
  return password && password[0] != '\0';
}

inline bool otaPasswordMatches(const char* configuredPassword, const char* suppliedPassword) {
  return otaPasswordConfigured(configuredPassword) && suppliedPassword && suppliedPassword[0] != '\0' &&
         strcmp(configuredPassword, suppliedPassword) == 0;
}

inline bool otaAuthAlertDue(unsigned long now, unsigned long& lastAlertMs,
                            unsigned long intervalMs = 60000) {
  if (lastAlertMs != 0 && now - lastAlertMs < intervalMs) {
    return false;
  }
  lastAlertMs = now;
  return true;
}

inline bool otaFilesystemNeedsSignatureRejection(bool isSketch, bool signingEnabled) {
  return !isSketch && signingEnabled;
}

#endif
