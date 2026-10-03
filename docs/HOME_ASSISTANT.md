# Home Assistant MQTT Integration & Dead-Man Watcher

This document details the Home Assistant integration for **ESP32 Poop Monitor**, including MQTT setup, entity mapping, Last Will and Testament (LWT) availability, and native offline alerting automations.

---

## 1. Architecture Overview

```
                      +-----------------------------+
                      |       ESP32-C3 Device       |
                      |  (Poop Monitor v4.0.2+)     |
                      +--------------+--------------+
                                     |
               MQTT Telemetry / LWT  |  (Keep-Alive: 60s, Status: 30s)
                                     v
                 +-----------------------------------+
                 |     Mosquitto Broker (HA Addon)   |
                 |       192.168.1.150:1883          |
                 +-------------------+---------------+
                                     |
                Internal Supervisor  |  (core-mosquitto)
                                     v
                 +-----------------------------------+
                 |       Home Assistant Core         |
                 |     - 18 Discovered Entities      |
                 |     - 5-Minute Inactivity Alert   |
                 |     - Recovery Notification       |
                 +-----------------------------------+
```

The ESP32 uses native MQTT auto-discovery (`homeassistant/#`) to register entities automatically with Home Assistant. No manual YAML entity definition is required.

---

## 2. Broker & Credentials Configuration

### Mosquitto Broker Add-on
In Home Assistant OS / Supervised:
* **Host:** `192.168.1.150` (or `homeassistant.local`)
* **Port:** `1883`
* **Internal HA Connection:** Home Assistant communicates with Mosquitto automatically using internal supervisor credentials (`broker: core-mosquitto`). *Do not change the credentials inside the Home Assistant MQTT integration UI.*
* **Device Client Credentials:** Defined in Mosquitto Broker configuration (`options.logins`):
  ```json
  [
    {
      "username": "poop",
      "password": "YOUR_MQTT_PASSWORD"
    }
  ]
  ```

### Firmware Credentials (`src/credentials.cpp`)
```cpp
// MQTT Configuration
const char* MQTT_USER = "poop";
const char* MQTT_PASSWORD = "YOUR_MQTT_PASSWORD";
```
*Note: `src/credentials.cpp` is gitignored to protect local network secrets.*

---

## 3. Discovered Entities (18 Total)

When the ESP32 boots and connects to MQTT, it publishes retained discovery payloads to `homeassistant/<component>/esp32_poop_monitor/<object_id>/config`.

### Sensors
| Entity ID | Friendly Name | State Topic | Unit / Type | Description |
|---|---|---|---|---|
| `sensor.esp32_poop_monitor_wifi_signal` | WiFi Signal | `homeassistant/sensor/poop_monitor/wifi_signal` | `dBm` | Live RSSI signal strength |
| `sensor.esp32_poop_monitor_wifi_quality` | WiFi Quality | `homeassistant/sensor/poop_monitor/wifi_quality` | String | Classified: Excellent, Good, Fair, Poor |
| `sensor.esp32_poop_monitor_wifi_ssid` | WiFi SSID | `homeassistant/sensor/poop_monitor/wifi_ssid` | String | Active network SSID |
| `sensor.esp32_poop_monitor_wifi_network_role` | WiFi Network Role | `homeassistant/sensor/poop_monitor/wifi_network` | String | `primary` or `secondary` (failover) |
| `sensor.esp32_poop_monitor_network_latency` | Network Latency | `homeassistant/sensor/poop_monitor/network_latency` | `ms` | HTTP RTT latency probe |
| `sensor.esp32_poop_monitor_network_jitter` | Network Jitter | `homeassistant/sensor/poop_monitor/network_jitter` | `ms` | Variation in probe latency |
| `sensor.esp32_poop_monitor_network_probe_target` | Network Probe Target | `homeassistant/sensor/poop_monitor/network_probe_target` | URL | Target endpoint used for latency checks |
| `sensor.esp32_poop_monitor_uptime` | Uptime | `homeassistant/sensor/poop_monitor/status` | `s` (duration) | Device running time in seconds |
| `sensor.esp32_poop_monitor_free_memory` | Free Memory | `homeassistant/sensor/poop_monitor/memory` | String | Free heap / total heap (`198KB/281KB`) |
| `sensor.esp32_poop_monitor_ip_address` | IP Address | `homeassistant/sensor/poop_monitor/status` | IP | Local LAN IPv4 address |
| `sensor.esp32_poop_monitor_firmware` | Firmware | `homeassistant/sensor/poop_monitor/status` | String | Installed firmware version (e.g. `4.0.2`) |
| `sensor.esp32_poop_monitor_status` | Status | `homeassistant/sensor/poop_monitor/status` | JSON | Full device telemetry JSON |
| `sensor.esp32_poop_monitor_last_heartbeat` | Last Heartbeat | `homeassistant/sensor/poop_monitor/status` | Duration | Time since last successful ping |
| `sensor.esp32_poop_monitor_telnet_log` | Telnet Log | `homeassistant/sensor/poop_monitor/telnet` | String | Live streamed console log line |

### Binary Sensors
| Entity ID | Friendly Name | Device Class | Description |
|---|---|---|---|
| `binary_sensor.esp32_poop_monitor_dns` | DNS | `connectivity` | `ON` if primary/fallback DNS resolves successfully, `OFF` if failing |
| `binary_sensor.esp32_poop_monitor_alerts_enabled` | Alerts Enabled | `diagnostic` | `ON` when alerts are active, `OFF` when temporarily paused |

### Controls
| Entity ID | Friendly Name | Command Topic | Description |
|---|---|---|---|
| `switch.esp32_poop_monitor_alert_control` | Alert Control | `homeassistant/poop_monitor/command/alerts` | Toggle alerts on/off remotely |
| `button.esp32_poop_monitor_reboot` | Reboot | `homeassistant/poop_monitor/command/reboot` | Sends remote reboot command to the device |

---

## 4. Availability & Last Will and Testament (LWT)

The firmware registers an MQTT Last Will and Testament on:
```
Topic: homeassistant/sensor/poop_monitor/availability
Payload on disconnect: offline (QoS 1, Retain: true)
Payload on connect: online (QoS 1, Retain: true)
```

Every sensor config registers:
`"availability_topic": "homeassistant/sensor/poop_monitor/availability"`

When the ESP32 loses power, drops Wi-Fi, or reboots, the Mosquitto broker immediately broadcasts `offline`. Home Assistant marks all 18 sensors as **`unavailable`** in the dashboard.

---

## 5. Home Assistant 5-Minute Inactivity Alert

To replace or complement an external dead-man watcher (`notification-api`), configure these automations in `/config/automations.yaml`:

```yaml
- id: esp32_poop_monitor_offline_alert
  alias: "ESP32 Poop Monitor Offline Alert (5 Minutes)"
  description: "Fires an alert if the ESP32 Poop Monitor stops communicating via MQTT for 5 minutes"
  trigger:
    - trigger: state
      entity_id: sensor.esp32_poop_monitor_wifi_signal
      to: unavailable
      for:
        minutes: 5
  action:
    - action: notify.notify
      data:
        title: "💩 ESP32 Poop Monitor Offline"
        message: "The ESP32 Poop Monitor has not sent an MQTT ping for 5 minutes."
    - action: persistent_notification.create
      data:
        title: "ESP32 Poop Monitor Offline"
        message: "The ESP32 Poop Monitor has stopped communicating over MQTT (offline for >5 minutes)."
        notification_id: "poop_monitor_offline"
  mode: single

- id: esp32_poop_monitor_online_recovery
  alias: "ESP32 Poop Monitor Online Recovery"
  description: "Notifies and dismisses alert when ESP32 Poop Monitor reconnects to MQTT"
  trigger:
    - trigger: state
      entity_id: sensor.esp32_poop_monitor_wifi_signal
      from: unavailable
  action:
    - action: persistent_notification.dismiss
      data:
        notification_id: "poop_monitor_offline"
    - action: notify.notify
      data:
        title: "✅ ESP32 Poop Monitor Online"
        message: "The ESP32 Poop Monitor is back online and communicating via MQTT."
  mode: single
```

### Reloading Automations
After editing `/config/automations.yaml`:
* **Via UI:** Developer Tools → YAML → Automations → Reload Automations
* **Via API / CLI:** Call service `automation.reload`
