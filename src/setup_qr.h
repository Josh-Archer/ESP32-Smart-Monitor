#ifndef SETUP_QR_H
#define SETUP_QR_H

#include <stddef.h>

// QR payload builders for the setup page.
//
// Wi-Fi passwords are caller-supplied and are never read from firmware
// credentials. Home Assistant payloads carry only public onboarding fields
// (name, dashboard URL, broker host/port, discovery prefix, device id).

#ifdef __cplusplus
extern "C" {
#endif

#define SETUP_QR_OK 0
#define SETUP_QR_ERR_INVALID (-1)
#define SETUP_QR_ERR_TOO_LONG (-2)
#define SETUP_QR_ERR_BUFFER (-3)

// WIFI:T:<WPA|WEP|nopass>;S:<ssid>;[P:<password>;]H:<true|false>;;
// An empty or NULL password, or auth "nopass", omits P: entirely.
// ssid is required, at most 32 bytes. password is at most 63 bytes.
int setup_qr_build_wifi(const char* ssid, const char* password, const char* auth,
                        int hidden, char* out, size_t out_len);

// Copies an http(s) URL with no userinfo. Rejects other schemes.
int setup_qr_build_dashboard(const char* url, char* out, size_t out_len);

// esp32mon://setup?v=1&name=&url=&mqtt=&discovery=&device=
// Empty optional fields are omitted. mqtt is "host" or "host:port".
// There is no parameter for a username, password, or token.
int setup_qr_build_ha(const char* name, const char* dashboard_url,
                      const char* mqtt_host, int mqtt_port,
                      const char* discovery_prefix, const char* device_id,
                      char* out, size_t out_len);

#ifdef __cplusplus
}
#endif

#endif
