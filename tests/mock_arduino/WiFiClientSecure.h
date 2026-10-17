#pragma once
#include "Arduino.h"

struct MockWiFiClientSecure {
  bool insecureCalled = false;
  unsigned long handshakeTimeout = 0;
  uint32_t timeoutSeconds = 0;
  static int instancesCreated;

  static int connectCalls;
  static std::string lastConnectHost;
  static IPAddress lastConnectIp;

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

  int connect(IPAddress ip, uint16_t, const char* host = nullptr, const char* = nullptr, const char* = nullptr, const char* = nullptr) {
    connectCalls++;
    lastConnectIp = ip;
    lastConnectHost = host ? host : "";
    return 1;
  }
};
inline int MockWiFiClientSecure::instancesCreated = 0;
inline int MockWiFiClientSecure::connectCalls = 0;
inline std::string MockWiFiClientSecure::lastConnectHost = "";
inline IPAddress MockWiFiClientSecure::lastConnectIp = IPAddress(0, 0, 0, 0);
using WiFiClientSecure = MockWiFiClientSecure;
