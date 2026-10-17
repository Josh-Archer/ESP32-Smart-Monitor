#include "setup_qr.h"

#include <stdio.h>
#include <string.h>

static int eq_ci(const char* a, const char* b) {
  if (!a || !b) {
    return 0;
  }
  while (*a && *b) {
    unsigned char ca = (unsigned char)*a++;
    unsigned char cb = (unsigned char)*b++;
    if (ca >= 'A' && ca <= 'Z') {
      ca = (unsigned char)(ca - 'A' + 'a');
    }
    if (cb >= 'A' && cb <= 'Z') {
      cb = (unsigned char)(cb - 'A' + 'a');
    }
    if (ca != cb) {
      return 0;
    }
  }
  return *a == '\0' && *b == '\0';
}

static int check_text(const char* s, size_t max_len) {
  size_t n = 0;
  if (!s) {
    return SETUP_QR_ERR_INVALID;
  }
  for (const unsigned char* p = (const unsigned char*)s; *p; ++p) {
    if (*p < 0x20 || *p == 0x7f) {
      return SETUP_QR_ERR_INVALID;
    }
    n++;
    if (n > max_len) {
      return SETUP_QR_ERR_TOO_LONG;
    }
  }
  return SETUP_QR_OK;
}

struct Buf {
  char* data;
  size_t cap;
  size_t len;
};

static int buf_add(Buf* b, char c) {
  if (b->len + 1 >= b->cap) {
    return SETUP_QR_ERR_BUFFER;
  }
  b->data[b->len++] = c;
  return SETUP_QR_OK;
}

static int buf_adds(Buf* b, const char* s) {
  while (*s) {
    int rc = buf_add(b, *s++);
    if (rc != SETUP_QR_OK) {
      return rc;
    }
  }
  return SETUP_QR_OK;
}

static int buf_finish(Buf* b) {
  if (b->len >= b->cap) {
    return SETUP_QR_ERR_BUFFER;
  }
  b->data[b->len] = '\0';
  return SETUP_QR_OK;
}

// Match JavaScript encodeURIComponent: leave A-Z a-z 0-9 - _ . ! ~ * ' ( )
static int is_component_unreserved(unsigned char c) {
  if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
    return 1;
  }
  switch (c) {
    case '-':
    case '_':
    case '.':
    case '!':
    case '~':
    case '*':
    case '\'':
    case '(':
    case ')':
      return 1;
    default:
      return 0;
  }
}

static int append_pct(Buf* b, const char* s) {
  static const char* hex = "0123456789ABCDEF";
  for (const unsigned char* p = (const unsigned char*)s; *p; ++p) {
    if (is_component_unreserved(*p)) {
      int rc = buf_add(b, (char)*p);
      if (rc != SETUP_QR_OK) {
        return rc;
      }
    } else {
      int rc = buf_add(b, '%');
      if (rc != SETUP_QR_OK) {
        return rc;
      }
      rc = buf_add(b, hex[*p >> 4]);
      if (rc != SETUP_QR_OK) {
        return rc;
      }
      rc = buf_add(b, hex[*p & 0x0f]);
      if (rc != SETUP_QR_OK) {
        return rc;
      }
    }
  }
  return SETUP_QR_OK;
}

static int escape_wifi_field(Buf* b, const char* s) {
  for (const unsigned char* p = (const unsigned char*)s; *p; ++p) {
    if (*p == '\\' || *p == ';' || *p == ',' || *p == ':' || *p == '"') {
      int rc = buf_add(b, '\\');
      if (rc != SETUP_QR_OK) {
        return rc;
      }
    }
    int rc = buf_add(b, (char)*p);
    if (rc != SETUP_QR_OK) {
      return rc;
    }
  }
  return SETUP_QR_OK;
}

static const char* normalize_auth(const char* auth, int have_password) {
  if (!have_password) {
    return "nopass";
  }
  if (auth && eq_ci(auth, "nopass")) {
    return "nopass";
  }
  if (auth && eq_ci(auth, "wep")) {
    return "WEP";
  }
  return "WPA";
}

int setup_qr_build_wifi(const char* ssid, const char* password, const char* auth,
                        int hidden, char* out, size_t out_len) {
  int rc;
  int have_password;
  const char* security;
  Buf buf;

  if (!out || out_len == 0) {
    return SETUP_QR_ERR_BUFFER;
  }
  out[0] = '\0';
  if (!ssid || ssid[0] == '\0') {
    return SETUP_QR_ERR_INVALID;
  }
  rc = check_text(ssid, 32);
  if (rc != SETUP_QR_OK) {
    return rc;
  }
  have_password = password && password[0] != '\0';
  if (have_password) {
    rc = check_text(password, 63);
    if (rc != SETUP_QR_OK) {
      return rc;
    }
  }

  security = normalize_auth(auth, have_password);
  if (eq_ci(security, "nopass")) {
    have_password = 0;
  }

  buf.data = out;
  buf.cap = out_len;
  buf.len = 0;

  rc = buf_adds(&buf, "WIFI:T:");
  if (rc != SETUP_QR_OK) {
    return rc;
  }
  rc = buf_adds(&buf, security);
  if (rc != SETUP_QR_OK) {
    return rc;
  }
  rc = buf_adds(&buf, ";S:");
  if (rc != SETUP_QR_OK) {
    return rc;
  }
  rc = escape_wifi_field(&buf, ssid);
  if (rc != SETUP_QR_OK) {
    return rc;
  }
  if (have_password) {
    rc = buf_adds(&buf, ";P:");
    if (rc != SETUP_QR_OK) {
      return rc;
    }
    rc = escape_wifi_field(&buf, password);
    if (rc != SETUP_QR_OK) {
      return rc;
    }
  }
  rc = buf_adds(&buf, hidden ? ";H:true;;" : ";H:false;;");
  if (rc != SETUP_QR_OK) {
    return rc;
  }
  return buf_finish(&buf);
}

int setup_qr_build_dashboard(const char* url, char* out, size_t out_len) {
  int rc;
  const char* rest;
  const char* p;

  if (!out || out_len == 0) {
    return SETUP_QR_ERR_BUFFER;
  }
  out[0] = '\0';
  rc = check_text(url, 200);
  if (rc != SETUP_QR_OK) {
    return rc;
  }
  if (strncmp(url, "https://", 8) == 0) {
    rest = url + 8;
  } else if (strncmp(url, "http://", 7) == 0) {
    rest = url + 7;
  } else {
    return SETUP_QR_ERR_INVALID;
  }
  if (*rest == '\0' || *rest == '/') {
    return SETUP_QR_ERR_INVALID;
  }
  if (strchr(url, '@') != NULL) {
    return SETUP_QR_ERR_INVALID;
  }
  for (p = url; *p; ++p) {
    if (*p == ' ' || *p == '"' || *p == '\'' || *p == '<' || *p == '>' || *p == '\\') {
      return SETUP_QR_ERR_INVALID;
    }
  }
  if (strlen(url) + 1 > out_len) {
    return SETUP_QR_ERR_BUFFER;
  }
  memcpy(out, url, strlen(url) + 1);
  return SETUP_QR_OK;
}

static int add_param(Buf* b, const char* key, const char* value) {
  int rc;
  if (!value || value[0] == '\0') {
    return SETUP_QR_OK;
  }
  rc = buf_add(b, '&');
  if (rc != SETUP_QR_OK) {
    return rc;
  }
  rc = buf_adds(b, key);
  if (rc != SETUP_QR_OK) {
    return rc;
  }
  rc = buf_add(b, '=');
  if (rc != SETUP_QR_OK) {
    return rc;
  }
  return append_pct(b, value);
}

int setup_qr_build_ha(const char* name, const char* dashboard_url,
                      const char* mqtt_host, int mqtt_port,
                      const char* discovery_prefix, const char* device_id,
                      char* out, size_t out_len) {
  int rc;
  char mqtt_value[320];
  Buf buf;
  const char* host = mqtt_host ? mqtt_host : "";

  if (!out || out_len == 0) {
    return SETUP_QR_ERR_BUFFER;
  }
  out[0] = '\0';

  if (name && name[0]) {
    rc = check_text(name, 64);
    if (rc != SETUP_QR_OK) {
      return rc;
    }
  }
  if (dashboard_url && dashboard_url[0]) {
    char scratch[201];
    rc = setup_qr_build_dashboard(dashboard_url, scratch, sizeof(scratch));
    if (rc != SETUP_QR_OK) {
      return rc;
    }
  }
  if (host[0]) {
    rc = check_text(host, 253);
    if (rc != SETUP_QR_OK) {
      return rc;
    }
    if (strchr(host, ':') || strchr(host, '/') || strchr(host, '@') || strchr(host, ' ')) {
      return SETUP_QR_ERR_INVALID;
    }
    if (mqtt_port < 0 || mqtt_port > 65535) {
      return SETUP_QR_ERR_INVALID;
    }
  } else if (mqtt_port < 0 || mqtt_port > 65535) {
    return SETUP_QR_ERR_INVALID;
  }
  if (discovery_prefix && discovery_prefix[0]) {
    rc = check_text(discovery_prefix, 64);
    if (rc != SETUP_QR_OK) {
      return rc;
    }
  }
  if (device_id && device_id[0]) {
    rc = check_text(device_id, 64);
    if (rc != SETUP_QR_OK) {
      return rc;
    }
  }

  mqtt_value[0] = '\0';
  if (host[0]) {
    int n;
    if (mqtt_port > 0) {
      n = snprintf(mqtt_value, sizeof(mqtt_value), "%s:%d", host, mqtt_port);
    } else {
      n = snprintf(mqtt_value, sizeof(mqtt_value), "%s", host);
    }
    if (n < 0 || (size_t)n >= sizeof(mqtt_value)) {
      return SETUP_QR_ERR_TOO_LONG;
    }
  }

  buf.data = out;
  buf.cap = out_len;
  buf.len = 0;
  rc = buf_adds(&buf, "esp32mon://setup?v=1");
  if (rc != SETUP_QR_OK) {
    return rc;
  }
  rc = add_param(&buf, "name", name);
  if (rc != SETUP_QR_OK) {
    return rc;
  }
  rc = add_param(&buf, "url", dashboard_url);
  if (rc != SETUP_QR_OK) {
    return rc;
  }
  rc = add_param(&buf, "mqtt", mqtt_value);
  if (rc != SETUP_QR_OK) {
    return rc;
  }
  rc = add_param(&buf, "discovery", discovery_prefix);
  if (rc != SETUP_QR_OK) {
    return rc;
  }
  rc = add_param(&buf, "device", device_id);
  if (rc != SETUP_QR_OK) {
    return rc;
  }
  return buf_finish(&buf);
}
