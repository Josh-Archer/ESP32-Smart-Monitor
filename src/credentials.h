#pragma once

// -----------------------------------------------------------------------------
// Credentials boundary
//
// Secrets come from src/credentials_private.h (copy credentials.template.h).
// Non-secret device config lives in config.cpp / config.h.
//
// Missing or incomplete private credentials fail at compile time with a clear
// #error — not a cryptic linker "undefined reference".
// -----------------------------------------------------------------------------

#if defined(__has_include)
#  if __has_include("credentials_private.h")
#    include "credentials_private.h"
#  endif
#endif

#ifndef WIFI_SSID
#  error "Missing WIFI_SSID. Copy credentials.template.h to src/credentials_private.h and set your secrets."
#endif
#ifndef WIFI_PASSWORD
#  error "Missing WIFI_PASSWORD. Copy credentials.template.h to src/credentials_private.h and set your secrets."
#endif
#ifndef OTA_PASSWORD
#  error "Missing OTA_PASSWORD. Copy credentials.template.h to src/credentials_private.h and set your secrets."
#endif
#ifndef PUSHOVER_TOKEN
#  error "Missing PUSHOVER_TOKEN. Copy credentials.template.h to src/credentials_private.h and set your secrets."
#endif
#ifndef PUSHOVER_USER
#  error "Missing PUSHOVER_USER. Copy credentials.template.h to src/credentials_private.h and set your secrets."
#endif
#ifndef MQTT_USER
#  error "Missing MQTT_USER. Copy credentials.template.h to src/credentials_private.h and set your secrets (use \"\" if unused)."
#endif
#ifndef MQTT_PASSWORD
#  error "Missing MQTT_PASSWORD. Copy credentials.template.h to src/credentials_private.h and set your secrets (use \"\" if unused)."
#endif
