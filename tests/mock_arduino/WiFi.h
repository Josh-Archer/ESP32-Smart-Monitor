#pragma once
#include "Arduino.h"
#include "IPAddress.h"

enum wl_status_t {
  WL_NO_SHIELD = 255,
  WL_IDLE_STATUS = 0,
  WL_NO_SSID_AVAIL = 1,
  WL_SCAN_COMPLETED = 2,
  WL_CONNECTED = 3,
  WL_CONNECT_FAILED = 4,
  WL_CONNECTION_LOST = 5,
  WL_DISCONNECTED = 6
};

struct MockWiFi {
  static inline unsigned long lastDnsTimeout = 0;
  static inline int hostByNameCalls = 0;
  wl_status_t mockStatus = WL_CONNECTED;
  wl_status_t status() { return mockStatus; }
  int hostByName(const char*, IPAddress& out, unsigned long timeoutMs) {
    ++hostByNameCalls;
    lastDnsTimeout = timeoutMs;
    out = IPAddress(1, 2, 3, 4);
    return 1;
  }
};
inline MockWiFi WiFi;
