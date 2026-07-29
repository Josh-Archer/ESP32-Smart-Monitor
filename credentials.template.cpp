// DEPRECATED: use credentials.template.h instead.
//
// Previous flow (no longer used by the build):
//   copy credentials.template.cpp → src/credentials.cpp
//
// Current flow:
//   1. Copy credentials.template.h → src/credentials_private.h
//   2. Fill in YOUR_* placeholders
//   3. Secrets are compile-time macros checked in src/credentials.h
//
// Non-secret settings (MQTT host, DNS, device name, etc.) remain in
// src/config.cpp. Do not put WiFi/OTA/Pushover/MQTT passwords there.
//
// This file is kept only so existing docs/links do not 404; it is not compiled.
