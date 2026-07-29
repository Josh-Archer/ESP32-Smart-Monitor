#include "network_metrics.h"
#include "config.h"
#include "telnet.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <math.h>
#include <string.h>

// Defaults: lightweight public HTTP endpoint used for connectivity checks
const char* NETWORK_DEFAULT_PROBE_TARGET = "http://httpbin.org/ip";
const unsigned long NETWORK_DEFAULT_PROBE_INTERVAL_MS = 60UL * 1000UL;  // 60s
const uint8_t NETWORK_DEFAULT_PROBE_SAMPLES = 4;
const unsigned long NETWORK_DEFAULT_PROBE_TIMEOUT_MS = 3000UL;

char networkProbeTarget[128];
unsigned long networkProbeIntervalMs = NETWORK_DEFAULT_PROBE_INTERVAL_MS;
uint8_t networkProbeSamples = NETWORK_DEFAULT_PROBE_SAMPLES;
unsigned long networkProbeTimeoutMs = NETWORK_DEFAULT_PROBE_TIMEOUT_MS;

float networkLatencyMs = -1.0f;
float networkJitterMs = -1.0f;
unsigned long lastNetworkProbeMs = 0;
bool networkProbeOk = false;
uint8_t networkProbeSuccessCount = 0;
uint8_t networkProbeAttemptCount = 0;

static bool configLoaded = false;

static void setDefaultProbeTarget() {
  strncpy(networkProbeTarget, NETWORK_DEFAULT_PROBE_TARGET, sizeof(networkProbeTarget) - 1);
  networkProbeTarget[sizeof(networkProbeTarget) - 1] = '\0';
}

static bool isValidHttpUrl(const char* url) {
  if (url == nullptr || url[0] == '\0') return false;
  size_t len = strlen(url);
  if (len < 10 || len >= sizeof(networkProbeTarget)) return false;
  // Only allow http:// to avoid TLS heap pressure on ESP32-C3 for background probes
  if (strncmp(url, "http://", 7) != 0) return false;
  // Basic sanity: host present after scheme
  if (url[7] == '\0' || url[7] == '/') return false;
  // Reject characters that break HTTPClient / storage
  for (size_t i = 0; i < len; i++) {
    char c = url[i];
    if (c < 32 || c > 126) return false;
    if (c == ' ' || c == '"' || c == '\'' || c == '\\') return false;
  }
  return true;
}

void loadNetworkMetricsConfigFromStorage() {
  if (configLoaded) return;

  setDefaultProbeTarget();
  networkProbeIntervalMs = NETWORK_DEFAULT_PROBE_INTERVAL_MS;
  networkProbeSamples = NETWORK_DEFAULT_PROBE_SAMPLES;
  networkProbeTimeoutMs = NETWORK_DEFAULT_PROBE_TIMEOUT_MS;

  Preferences prefs;
  if (!prefs.begin("net_metrics", true)) {
    Serial.println("[NET] No stored network metrics config, using defaults");
    configLoaded = true;
    return;
  }

  String target = prefs.getString("probe_tgt", NETWORK_DEFAULT_PROBE_TARGET);
  unsigned long interval = prefs.getULong("interval", NETWORK_DEFAULT_PROBE_INTERVAL_MS);
  uint8_t samples = (uint8_t)prefs.getUChar("samples", NETWORK_DEFAULT_PROBE_SAMPLES);
  unsigned long timeout = prefs.getULong("timeout", NETWORK_DEFAULT_PROBE_TIMEOUT_MS);
  prefs.end();

  if (isValidHttpUrl(target.c_str())) {
    strncpy(networkProbeTarget, target.c_str(), sizeof(networkProbeTarget) - 1);
    networkProbeTarget[sizeof(networkProbeTarget) - 1] = '\0';
  } else {
    setDefaultProbeTarget();
  }

  const unsigned long MAX_INTERVAL = 24UL * 60UL * 60UL * 1000UL;
  if (interval >= 10000UL && interval <= MAX_INTERVAL) {
    networkProbeIntervalMs = interval;
  }
  if (samples >= 2 && samples <= 10) {
    networkProbeSamples = samples;
  }
  if (timeout >= 500UL && timeout <= 15000UL) {
    networkProbeTimeoutMs = timeout;
  }

  configLoaded = true;
  Serial.printf("[NET] Loaded config: target=%s interval=%lu ms samples=%u timeout=%lu ms\r\n",
                networkProbeTarget, networkProbeIntervalMs, networkProbeSamples, networkProbeTimeoutMs);
}

void saveNetworkMetricsConfigToStorage() {
  Preferences prefs;
  if (!prefs.begin("net_metrics", false)) {
    Serial.println("[NET] Failed to open NVS for network metrics save");
    return;
  }
  prefs.putString("probe_tgt", networkProbeTarget);
  prefs.putULong("interval", networkProbeIntervalMs);
  prefs.putUChar("samples", networkProbeSamples);
  prefs.putULong("timeout", networkProbeTimeoutMs);
  prefs.end();
  Serial.println("[NET] Network metrics config saved to NVS");
}

void updateNetworkMetricsConfig(const char* probeTarget,
                                unsigned long intervalMs,
                                uint8_t samples,
                                unsigned long timeoutMs) {
  bool changed = false;
  const unsigned long MAX_INTERVAL = 24UL * 60UL * 60UL * 1000UL;

  if (probeTarget != nullptr && probeTarget[0] != '\0') {
    if (isValidHttpUrl(probeTarget)) {
      if (strncmp(networkProbeTarget, probeTarget, sizeof(networkProbeTarget)) != 0) {
        strncpy(networkProbeTarget, probeTarget, sizeof(networkProbeTarget) - 1);
        networkProbeTarget[sizeof(networkProbeTarget) - 1] = '\0';
        changed = true;
      }
    } else {
      Serial.printf("[NET] Rejected invalid probe target: %s\r\n", probeTarget);
    }
  }

  if (intervalMs != 0) {
    if (intervalMs >= 10000UL && intervalMs <= MAX_INTERVAL) {
      if (intervalMs != networkProbeIntervalMs) {
        networkProbeIntervalMs = intervalMs;
        changed = true;
      }
    } else {
      Serial.printf("[NET] Rejected invalid interval_ms: %lu\r\n", intervalMs);
    }
  }

  if (samples != 0) {
    if (samples >= 2 && samples <= 10) {
      if (samples != networkProbeSamples) {
        networkProbeSamples = samples;
        changed = true;
      }
    } else {
      Serial.printf("[NET] Rejected invalid samples: %u\r\n", samples);
    }
  }

  if (timeoutMs != 0) {
    if (timeoutMs >= 500UL && timeoutMs <= 15000UL) {
      if (timeoutMs != networkProbeTimeoutMs) {
        networkProbeTimeoutMs = timeoutMs;
        changed = true;
      }
    } else {
      Serial.printf("[NET] Rejected invalid timeout_ms: %lu\r\n", timeoutMs);
    }
  }

  if (changed) {
    Serial.printf("[NET] Updated config: target=%s interval=%lu ms samples=%u timeout=%lu ms\r\n",
                  networkProbeTarget, networkProbeIntervalMs, networkProbeSamples, networkProbeTimeoutMs);
    saveNetworkMetricsConfigToStorage();
  } else {
    Serial.println("[NET] updateNetworkMetricsConfig called but no values changed");
  }
}

// Single HTTP RTT sample in milliseconds; returns -1 on failure
static float measureHttpRttMs(const char* url) {
  WiFiClient client;
  HTTPClient http;

  unsigned long start = millis();
  if (!http.begin(client, url)) {
    return -1.0f;
  }
  http.setTimeout((int)networkProbeTimeoutMs);
  http.setReuse(false);

  int code = http.GET();
  unsigned long elapsed = millis() - start;
  http.end();

  // Any completed HTTP exchange counts for latency (including 4xx/5xx);
  // connection/timeout failures return negative codes from HTTPClient.
  if (code > 0) {
    return (float)elapsed;
  }
  return -1.0f;
}

bool probeNetworkQuality() {
  if (WiFi.status() != WL_CONNECTED) {
    networkProbeOk = false;
    return false;
  }

  if (networkProbeTarget[0] == '\0') {
    setDefaultProbeTarget();
  }

  const uint8_t n = networkProbeSamples;
  float samples[10];
  uint8_t okCount = 0;

  Serial.printf("[%10lu ms] [NET] Probing latency/jitter target=%s samples=%u\r\n",
                millis(), networkProbeTarget, n);

  for (uint8_t i = 0; i < n && i < 10; i++) {
    float rtt = measureHttpRttMs(networkProbeTarget);
    networkProbeAttemptCount = i + 1;
    if (rtt >= 0.0f) {
      samples[okCount++] = rtt;
      Serial.printf("[%10lu ms] [NET] Sample %u: %.0f ms\r\n", millis(), i + 1, rtt);
    } else {
      Serial.printf("[%10lu ms] [NET] Sample %u: failed\r\n", millis(), i + 1);
    }
    // Small gap between samples to avoid hammering the target
    if (i + 1 < n) {
      delay(50);
    }
  }

  networkProbeSuccessCount = okCount;
  lastNetworkProbeMs = millis();

  if (okCount == 0) {
    networkProbeOk = false;
    networkLatencyMs = -1.0f;
    networkJitterMs = -1.0f;
    Serial.printf("[%10lu ms] [NET] Probe failed — no successful samples\r\n", millis());
    return false;
  }

  // Mean latency
  float sum = 0.0f;
  for (uint8_t i = 0; i < okCount; i++) {
    sum += samples[i];
  }
  networkLatencyMs = sum / (float)okCount;

  // Jitter: mean absolute difference between consecutive successful samples
  if (okCount >= 2) {
    float jsum = 0.0f;
    for (uint8_t i = 1; i < okCount; i++) {
      jsum += fabsf(samples[i] - samples[i - 1]);
    }
    networkJitterMs = jsum / (float)(okCount - 1);
  } else {
    networkJitterMs = 0.0f;
  }

  networkProbeOk = true;
  Serial.printf("[%10lu ms] [NET] Latency=%.1f ms Jitter=%.1f ms (%u/%u samples)\r\n",
                millis(), networkLatencyMs, networkJitterMs, okCount, n);
  return true;
}

void handleNetworkMetrics() {
  if (!configLoaded) {
    loadNetworkMetricsConfigFromStorage();
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();
  // First run soon after boot (after 5s), then on interval
  bool due = (lastNetworkProbeMs == 0)
             ? (now >= 5000UL)
             : ((now - lastNetworkProbeMs) >= networkProbeIntervalMs);

  if (due) {
    probeNetworkQuality();
  }
}
