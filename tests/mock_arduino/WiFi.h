#pragma once
#include "Arduino.h"

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
  wl_status_t mockStatus = WL_CONNECTED;
  wl_status_t status() { return mockStatus; }
};
inline MockWiFi WiFi;
