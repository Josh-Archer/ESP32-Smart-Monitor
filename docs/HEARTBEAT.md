# Heartbeat / notification-api Contract

The firmware periodically issues an HTTP **GET** to a configurable endpoint so an external watcher can detect device silence (and alert via Pushover, etc.).

This document describes how the ESP32 builds that URL, what it expects from the server, and how to point the device at **notification-api** or a generic health probe.

## Configuration (firmware)

Set these in `src/config.cpp`:

| Variable | Meaning | Example |
|---|---|---|
| `heartbeatBaseUrl` | Scheme + host (+ optional port). **No trailing slash.** | `http://notifications.example.com` |
| `heartbeatDeviceId` | Path segment used by notification-api | `poop`, `garage`, `lab-esp32` |
| `heartbeatPath` | Optional full path override. Empty = derive path. | `""` or `"/health"` |

Resolved URL (`getHeartbeatEndpoint()`):

1. If `heartbeatPath` is **non-empty**: `{heartbeatBaseUrl}{heartbeatPath}`
2. Else: `{heartbeatBaseUrl}/heartbeat/{heartbeatDeviceId}`

`deviceName` (e.g. `poop-monitor`) is separate: it is used for mDNS / identity, **not** for the heartbeat path unless you choose matching values.

### Default (notification-api legacy)

```cpp
const char* heartbeatBaseUrl = "http://notifications.archerfamily.io";
const char* heartbeatDeviceId = "poop";
const char* heartbeatPath = "";  // → /heartbeat/poop
// Resolved: http://notifications.archerfamily.io/heartbeat/poop
```

### Example: different device id on notification-api

```cpp
const char* heartbeatBaseUrl = "http://notifications.example.com";
const char* heartbeatDeviceId = "garage";
const char* heartbeatPath = "";
// Resolved: http://notifications.example.com/heartbeat/garage
```

> **Server note:** stock notification-api currently registers a hard-coded route
> `GET /heartbeat/poop`. Deployments that need other device ids must expose a
> matching route (or a parameterized `/heartbeat/{id}`) on the API side.

### Example: generic health endpoint

Use any probe that returns **HTTP 200** on success (Kubernetes/Traefik/nginx health, uptime-kuma push/http, a static `/health`, etc.):

```cpp
const char* heartbeatBaseUrl = "http://status.example.com";
const char* heartbeatDeviceId = "unused-when-path-set";
const char* heartbeatPath = "/health";
// Resolved: http://status.example.com/health
```

Other path examples: `"/ping"`, `"/api/v1/health"`, `"/readyz"`.

## HTTP contract (device → server)

| Item | Behavior |
|---|---|
| Method | `GET` |
| URL | Value of `getHeartbeatEndpoint()` |
| Body | None |
| Timeout | 10 seconds (firmware) |
| Interval | ~5 seconds between attempts (main loop delay) |
| Success | HTTP status **200** (response body is logged only; content is not parsed) |
| Failure | Non-200 or transport error; response code stored for status/MQTT |

The device does **not** send auth headers or a JSON body. Keep the endpoint simple and unauthenticated only if appropriate for your network threat model (prefer private network / reverse-proxy ACL).

## notification-api behavior (server → operator)

Reference implementation: [Josh-Archer/notification-api](https://github.com/Josh-Archer/notification-api).

| Item | Behavior |
|---|---|
| Route (current) | `GET /heartbeat/poop` |
| Response | `200` with body `OK` |
| Side effect | Updates in-memory last-seen timestamp |
| Silence alert | If no successful heartbeat for `HEARTBEAT_TIMEOUT_SECS` (default **90**), send Pushover alert |
| Check cadence | Server loop interval `CHECK_INTERVAL_SECS` (default **10**) |

Implication for the device: with a ~5s ping interval and a 90s timeout, a few dropped packets will not false-alarm; sustained disconnects will.

## Observability on the device

- Telnet: `[Heartbeat] Ping Response (200): ...` / failure strings
- Web/MQTT status (when enabled): `heartbeat_endpoint`, `heartbeat_base_url`, `heartbeat_device_id`, `heartbeat_path`, last success/code

## Checklist when changing the URL

1. Update `heartbeatBaseUrl` / `heartbeatDeviceId` / `heartbeatPath` in `src/config.cpp`.
2. Ensure the server route matches (notification-api path or your generic health path).
3. Rebuild and flash; confirm 200 responses in telnet or `/status`.
4. Confirm the watcher’s silence timeout is greater than a few missed intervals.
