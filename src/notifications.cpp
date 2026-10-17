#include "notifications.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <string>

#if defined(ARDUINO)
#include <lwip/dns.h>
#include <lwip/ip_addr.h>

static volatile bool s_dnsDone = false;
static volatile bool s_dnsActive = false;
static volatile unsigned long s_dnsGeneration = 0;
static ip_addr_t s_dnsResolvedAddr;

static void s_dnsFoundCallback(const char*, const ip_addr_t* ipaddr, void* callbackArg) {
  unsigned long generation = (unsigned long)(uintptr_t)callbackArg;
  if (!s_dnsActive || generation != s_dnsGeneration) {
    return;
  }
  if (ipaddr) {
    s_dnsResolvedAddr = *ipaddr;
  }
  s_dnsDone = true;
}

static bool resolveHostWithTimeout(const char* host, IPAddress& outIp, unsigned long timeoutMs = 3000) {
  if (!host || !*host) {
    return false;
  }

  ip_addr_t addr;
  unsigned long generation = ++s_dnsGeneration;
  s_dnsActive = true;
  s_dnsDone = false;
  err_t err = dns_gethostbyname(host, &addr, s_dnsFoundCallback,
                                (void*)(uintptr_t)generation);
  if (err == ERR_OK) {
    s_dnsActive = false;
    if (!IP_IS_V4(&addr)) {
      return false;
    }
    outIp = addr.u_addr.ip4.addr;
    return true;
  }
  if (err != ERR_INPROGRESS) {
    s_dnsActive = false;
    return false;
  }

  unsigned long start = millis();
  while (!s_dnsDone && (millis() - start) < timeoutMs) {
    delay(10);
  }
  s_dnsActive = false;
  if (!s_dnsDone || !IP_IS_V4(&s_dnsResolvedAddr)) {
    return false;
  }
  outIp = s_dnsResolvedAddr.u_addr.ip4.addr;
  return true;
}
#else
static bool resolveHostWithTimeout(const char* host, IPAddress& outIp, unsigned long timeoutMs = 3000) {
  return host && *host && WiFi.hostByName(host, outIp, timeoutMs) == 1;
}
#endif

String extractHostFromUrl(const char* url) {
  if (!url) {
    return "";
  }
  const char* p = strstr(url, "://");
  const char* start = p ? (p + 3) : url;
  const char* slash = strchr(start, '/');
  if (!slash) {
    return String(start);
  }
  std::string host(start, slash - start);
  return String(host.c_str());
}

String urlEncode(const char* str) {
  if (!str) {
    return "";
  }
  String encoded;
  encoded.reserve(strlen(str) * 3 / 2);
  const char* p = str;
  while (*p) {
    char c = *p++;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
        c == '-' || c == '.' || c == '_' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += '+';
    } else {
      char hex[4];
      snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
      encoded += hex;
    }
  }
  return encoded;
}

void sendPushoverAlert(const char* title, const char* message, int priority) {
  if (!pushoverToken || !*pushoverToken || !pushoverUser || !*pushoverUser) {
    Serial.printf("[%10lu ms] [PUSHOVER] Pushover disabled - token or user not configured\r\n", millis());
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[%10lu ms] [PUSHOVER] Cannot send alert - no WiFi\r\n", millis());
    return;
  }

  String host = extractHostFromUrl(pushoverApiUrl);
  IPAddress resolvedIp;
  if (!resolveHostWithTimeout(host.c_str(), resolvedIp, 3000)) {
    Serial.printf("[%10lu ms] [PUSHOVER] DNS resolution timed out or failed for %s\r\n", millis(), host.c_str());
    return;
  }

  WiFiClientSecure client;
  client.setInsecure(); // For simplicity - in production, use proper certificates
  client.setHandshakeTimeout(3);
  client.setTimeout(3);

  if (!client.connect(resolvedIp, 443, host.c_str(), nullptr, nullptr, nullptr)) {
    Serial.printf("[%10lu ms] [PUSHOVER] Failed to connect to %s (%s)\r\n", millis(), host.c_str(), resolvedIp.toString().c_str());
    return;
  }

  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(3000);

  if (http.begin(client, pushoverApiUrl)) {
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "token=" + urlEncode(pushoverToken) +
                     "&user=" + urlEncode(pushoverUser) +
                     "&title=" + urlEncode(title) +
                     "&message=" + urlEncode(message) +
                     "&priority=" + String(priority) +
                     "&device=" + urlEncode(deviceName);

    Serial.printf("[%10lu ms] [PUSHOVER] Sending alert: %s\r\n", millis(), title ? title : "");

    int httpCode = http.POST(postData);

    if (httpCode > 0) {
      String response = http.getString();
      Serial.printf("[%10lu ms] [PUSHOVER] Alert sent successfully (%d): %s\r\n", millis(), httpCode, response.c_str());
    } else {
      Serial.printf("[%10lu ms] [PUSHOVER] Failed to send alert: %s\r\n", millis(), http.errorToString(httpCode).c_str());
    }

    http.end();
  } else {
    Serial.printf("[%10lu ms] [PUSHOVER] Failed to connect to Pushover API\r\n", millis());
  }
}

