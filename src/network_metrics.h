#ifndef NETWORK_METRICS_H
#define NETWORK_METRICS_H

#include <Arduino.h>

// Default probe target (lightweight HTTP endpoint; override via MQTT/NVS)
extern const char* NETWORK_DEFAULT_PROBE_TARGET;
extern const unsigned long NETWORK_DEFAULT_PROBE_INTERVAL_MS;
extern const uint8_t NETWORK_DEFAULT_PROBE_SAMPLES;
extern const unsigned long NETWORK_DEFAULT_PROBE_TIMEOUT_MS;

// Runtime configuration
extern char networkProbeTarget[128];
extern unsigned long networkProbeIntervalMs;
extern uint8_t networkProbeSamples;
extern unsigned long networkProbeTimeoutMs;

// Latest measurements (-1 means unknown / no successful sample yet)
extern float networkLatencyMs;
extern float networkJitterMs;
extern unsigned long lastNetworkProbeMs;
extern bool networkProbeOk;
extern uint8_t networkProbeSuccessCount;
extern uint8_t networkProbeAttemptCount;

// Lifecycle
void loadNetworkMetricsConfigFromStorage();
void saveNetworkMetricsConfigToStorage();

// Update config; empty/null probeTarget leaves target unchanged; 0 leaves numeric fields unchanged
void updateNetworkMetricsConfig(const char* probeTarget,
                                unsigned long intervalMs,
                                uint8_t samples,
                                unsigned long timeoutMs);

// Run a multi-sample HTTP RTT probe; updates latency/jitter globals
// Returns true if at least one sample succeeded
bool probeNetworkQuality();

// Call from main loop — probes on interval when WiFi is up
void handleNetworkMetrics();

#endif
