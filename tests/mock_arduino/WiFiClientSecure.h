#pragma once
#include "Arduino.h"

struct MockWiFiClientSecure {
  bool insecureCalled = false;
  unsigned long handshakeTimeout = 0;
  uint32_t timeoutSeconds = 0;
  static int instancesCreated;

  MockWiFiClientSecure() {
    instancesCreated++;
  }

  void setInsecure() {
    insecureCalled = true;
  }

  void setHandshakeTimeout(unsigned long t) {
    handshakeTimeout = t;
  }

  int setTimeout(uint32_t s) {
    timeoutSeconds = s;
    return 0;
  }
};
inline int MockWiFiClientSecure::instancesCreated = 0;
using WiFiClientSecure = MockWiFiClientSecure;
