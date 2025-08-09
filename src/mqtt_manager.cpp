#ifdef ENABLE_MQTT

#include "telnet.h"
#include "mqtt_manager.h"
#include "config.h"
#include "dns_manager.h"
#include "system_utils.h"
#include <WiFi.h>

// MQTT client instances
WiFiClient wifiClientMQTT;
PubSubClient mqttClient(wifiClientMQTT);

// MQTT Topics - Using device name for unique identification
const char* MQTT_DEVICE_TOPIC = "homeassistant/sensor/poop_monitor";
const char* MQTT_STATUS_TOPIC = "homeassistant/sensor/poop_monitor/status";
const char* MQTT_AVAILABILITY_TOPIC = "homeassistant/sensor/poop_monitor/availability";
const char* MQTT_TELNET_TOPIC = "homeassistant/sensor/poop_monitor/telnet";
const char* MQTT_COMMAND_TOPIC = "homeassistant/poop_monitor/command";
const char* MQTT_DISCOVERY_PREFIX = "homeassistant";

// Home Assistant Device Info
const char* HA_DEVICE_NAME = "ESP32 Poop Monitor";
const char* HA_DEVICE_ID = "esp32_poop_monitor";
const char* HA_MANUFACTURER = "Custom";
const char* HA_MODEL = "ESP32-C3";

// Timing variables
unsigned long lastMQTTReconnectAttempt = 0;
unsigned long lastStatusPublish = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;    // Try to reconnect every 5 seconds
const unsigned long STATUS_PUBLISH_INTERVAL = 30000;   // Publish status every 30 seconds

// Helper to format memory usage as "freeKB/totalKB"
static String getMemoryUsage() {
    float freeKB = ESP.getFreeHeap() / 1024.0;
    float totalKB = ESP.getHeapSize() / 1024.0;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0fKB/%.0fKB", freeKB, totalKB);
    return String(buf);
}

// Publish all sensor states (not just discovery)
static void publishMetricsIndividual() {
    // WiFi Signal
    mqttClient.publish("homeassistant/sensor/poop_monitor/wifi_signal", String(WiFi.RSSI()).c_str(), false);
    mqttClient.publish("homeassistant/sensor/poop_monitor/wifi_quality", classifyWiFiSignal(WiFi.RSSI()), false);
    // DNS
    extern bool isDNSWorking;
    mqttClient.publish("homeassistant/sensor/poop_monitor/dns_status", isDNSWorking ? "ON" : "OFF", false);
    // Uptime seconds
    mqttClient.publish("homeassistant/sensor/poop_monitor/uptime", String(millis() / 1000).c_str(), false);
    // Free Memory
    mqttClient.publish("homeassistant/sensor/poop_monitor/memory", getMemoryUsage().c_str(), false);
    // IP Address
    mqttClient.publish("homeassistant/sensor/poop_monitor/ip_address", WiFi.localIP().toString().c_str(), false);
    // Firmware
    mqttClient.publish("homeassistant/sensor/poop_monitor/firmware", firmwareVersion, false);
    // Alerts
    extern bool areAlertsPaused();
    mqttClient.publish("homeassistant/sensor/poop_monitor/alerts", areAlertsPaused() ? "OFF" : "ON", false);
}

void publishAllSensors() {
    // Publish consolidated device status JSON
    String statusJson = getDeviceStatusJSON();
    mqttClient.publish(MQTT_STATUS_TOPIC, statusJson.c_str(), false);
    // Also publish individual topics for legacy consumers and debugging visibility
    publishMetricsIndividual();
}
void initializeMQTT() {
    Serial.println("Initializing MQTT...");
    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setKeepAlive(60);
    mqttClient.setSocketTimeout(30);
    mqttClient.setCallback(onMQTTMessage);  // Set callback for incoming messages
    
    // Set a larger buffer size for Home Assistant discovery messages
    mqttClient.setBufferSize(1024);
    
    Serial.printf("MQTT Server: %s:%d\n", mqttServer, mqttPort);
}

void connectToMQTT() {
    if (WiFi.status() != WL_CONNECTED) {
        return; // Don't try to connect to MQTT if WiFi is down
    }
    
    unsigned long now = millis();
    if (now - lastMQTTReconnectAttempt < MQTT_RECONNECT_INTERVAL) {
        return; // Don't try too frequently
    }
    lastMQTTReconnectAttempt = now;
    
    Serial.print("Attempting MQTT connection...");
    
    // Create a unique client ID
    String clientId = String(HA_DEVICE_ID) + "_" + String(WiFi.macAddress());
    clientId.replace(":", "");
    
    // Set Last Will and Testament
    const char* willTopic = MQTT_AVAILABILITY_TOPIC;
    const char* willMessage = "offline";
    
    bool connected = false;
    if (strlen(mqttUser) > 0 && strlen(mqttPassword) > 0) {
        // Connect with authentication
        connected = mqttClient.connect(clientId.c_str(), mqttUser, mqttPassword, 
                                     willTopic, 1, true, willMessage);
    } else {
        // Connect without authentication
        connected = mqttClient.connect(clientId.c_str(), willTopic, 1, true, willMessage);
    }
    
    if (connected) {
        Serial.println(" connected!");
        
        // Subscribe to command topics
        mqttClient.subscribe((String(MQTT_COMMAND_TOPIC) + "/reboot").c_str());
        mqttClient.subscribe((String(MQTT_COMMAND_TOPIC) + "/alerts").c_str());
        
        // Publish that we're online
        publishAvailability(true);
        
        // Publish Home Assistant discovery configuration
        publishHomeAssistantDiscovery();
        
        // Publish initial status
        publishAllSensors();
        
        Serial.println("MQTT setup complete with Home Assistant discovery");
    } else {
        Serial.printf(" failed, rc=%d. Retrying in %d seconds.\n", 
                     mqttClient.state(), MQTT_RECONNECT_INTERVAL / 1000);
    }
}

void publishHomeAssistantDiscovery() {
    Serial.println("Publishing Home Assistant auto-discovery configuration...");
    
    // Device information that will be shared across all entities
    JsonDocument deviceDoc;
    deviceDoc["identifiers"][0] = HA_DEVICE_ID;
    deviceDoc["name"] = HA_DEVICE_NAME;
    deviceDoc["manufacturer"] = HA_MANUFACTURER;
    deviceDoc["model"] = HA_MODEL;
    deviceDoc["sw_version"] = firmwareVersion;
    
    // 1. Main status sensor
        publishSensor("sensor", "status", "Status", 
                  nullptr, nullptr, MQTT_STATUS_TOPIC, "mdi:monitor");
    
    // 2. WiFi Signal Strength
        publishSensor("sensor", "wifi_signal", "WiFi Signal",
                  "dBm", "signal_strength", "homeassistant/sensor/poop_monitor/wifi_signal", "mdi:wifi");
        publishSensor("sensor", "wifi_quality", "WiFi Quality",
                  nullptr, nullptr, "homeassistant/sensor/poop_monitor/wifi_quality", "mdi:wifi");
    
    // 3. DNS Status (reads from consolidated status topic)
        publishSensor("binary_sensor", "dns", "DNS", 
                  nullptr, "connectivity", MQTT_STATUS_TOPIC, "mdi:dns");
    
    // 4. Uptime (reads from consolidated status topic)
        publishSensor("sensor", "uptime", "Uptime", 
                  "s", "duration", MQTT_STATUS_TOPIC, "mdi:clock");
    
    // 5. Memory Usage
        publishSensor("sensor", "free_memory", "Free Memory",
                  nullptr, nullptr, "homeassistant/sensor/poop_monitor/memory", "mdi:memory");
    
    // 6. Last Heartbeat Success
        publishSensor("sensor", "last_heartbeat", "Last Heartbeat",
                  nullptr, nullptr, MQTT_STATUS_TOPIC, "mdi:heart-pulse");
    
    // 7. IP Address (reads from consolidated status topic)
        publishSensor("sensor", "ip_address", "IP Address", 
                  nullptr, nullptr, MQTT_STATUS_TOPIC, "mdi:ip");
    
    // 8. Firmware Version (reads from consolidated status topic)
        publishSensor("sensor", "firmware", "Firmware", 
                  nullptr, nullptr, MQTT_STATUS_TOPIC, "mdi:chip");
    
    // 9. Alerts Status (reads from consolidated status topic)
        publishSensor("binary_sensor", "alerts", "Alerts Enabled", 
                  nullptr, nullptr, MQTT_STATUS_TOPIC, "mdi:bell");
    
    // 10. Telnet Log Sensor
        publishSensor("sensor", "telnet_log", "Telnet Log", 
                  nullptr, nullptr, MQTT_TELNET_TOPIC, "mdi:console");
    
    // 11. Alert Control Switch
        publishSwitch("alert_switch", "Alert Control", 
                  (String(MQTT_COMMAND_TOPIC) + "/alerts").c_str(),
                  "homeassistant/sensor/poop_monitor/alerts", "mdi:bell");
    
    // 12. Reboot Button
        publishButton("reboot", "Reboot", 
                  (String(MQTT_COMMAND_TOPIC) + "/reboot").c_str(), "mdi:restart");
    
    Serial.println("Home Assistant discovery configuration published");
}

void publishSensor(const char* component, const char* object_id, const char* name, 
                   const char* unit_of_measurement, const char* device_class, 
                   const char* state_topic, const char* icon) {
    
    // Build discovery topic
    String discoveryTopic = String(MQTT_DISCOVERY_PREFIX) + "/" + component + "/" + 
                           HA_DEVICE_ID + "/" + object_id + "/config";
    
    // Build configuration JSON
    JsonDocument configDoc;
    configDoc["name"] = name;
    configDoc["unique_id"] = String(HA_DEVICE_ID) + "_" + object_id;
    configDoc["state_topic"] = state_topic;
    configDoc["availability_topic"] = MQTT_AVAILABILITY_TOPIC;
    
    if (unit_of_measurement) {
        configDoc["unit_of_measurement"] = unit_of_measurement;
    }
    if (device_class) {
        configDoc["device_class"] = device_class;
    }
    if (icon) {
        configDoc["icon"] = icon;
    }
    
    // Add device information
    configDoc["device"]["identifiers"][0] = HA_DEVICE_ID;
    configDoc["device"]["name"] = HA_DEVICE_NAME;
    configDoc["device"]["manufacturer"] = HA_MANUFACTURER;
    configDoc["device"]["model"] = HA_MODEL;
    configDoc["device"]["sw_version"] = firmwareVersion;
    
    // For status sensor, add value template to extract specific values
    if (strcmp(object_id, "status") == 0) {
        configDoc["json_attributes_topic"] = state_topic;
        configDoc["value_template"] = "{{ value_json.status | default('online') }}";
    } else if (strcmp(object_id, "wifi_signal") == 0) {
        configDoc["value_template"] = "{{ value | float }}";
    } else if (strcmp(object_id, "wifi_quality") == 0) {
        configDoc["value_template"] = "{{ value | default('unknown') }}";
    } else if (strcmp(object_id, "dns") == 0) {
        configDoc["value_template"] = "{{ 'ON' if value_json.dns_working else 'OFF' }}";
        configDoc["payload_on"] = "ON";
        configDoc["payload_off"] = "OFF";
    } else if (strcmp(object_id, "uptime") == 0) {
        configDoc["value_template"] = "{{ (value_json.uptime_ms / 1000) | round(0) }}";
    } else if (strcmp(object_id, "free_memory") == 0) {
        configDoc["value_template"] = "{{ value | default('0KB/0KB') }}";
    } else if (strcmp(object_id, "free_memory_percent") == 0) {
        configDoc["value_template"] = "{{ value_json.free_memory_percent | default(0) }}";
    } else if (strcmp(object_id, "last_heartbeat") == 0) {
        configDoc["value_template"] = "{{ value_json.last_heartbeat_formatted | default('Never') }}";
    } else if (strcmp(object_id, "ip_address") == 0) {
        configDoc["value_template"] = "{{ value_json.ip_address | default('unknown') }}";
    } else if (strcmp(object_id, "firmware") == 0) {
        configDoc["value_template"] = "{{ value_json.firmware_version | default('unknown') }}";
    } else if (strcmp(object_id, "alerts") == 0) {
        configDoc["value_template"] = "{{ 'OFF' if value_json.alerts_paused else 'ON' }}";
        configDoc["payload_on"] = "ON";
        configDoc["payload_off"] = "OFF";
    }
    
    // Serialize and publish
    String configJson;
    configJson.reserve(768);
    serializeJson(configDoc, configJson);
    
    telnetPrintf("[MQTT] Publishing discovery for %s to topic: %s\nPayload: %s\n", object_id, discoveryTopic.c_str(), configJson.c_str());
    bool success = mqttClient.publish(discoveryTopic.c_str(), configJson.c_str(), true);
    Serial.printf("Discovery %s: %s (%s)\n", 
                  success ? "OK" : "FAILED", 
                  object_id, 
                  discoveryTopic.c_str());
}

void publishDeviceStatus() {
    if (!mqttClient.connected()) {
        return;
    }
    
    // Get comprehensive status JSON
    String statusJson = getDeviceStatusJSON();
    
    // Publish main status
    telnetPrintf("[MQTT] Publishing device status to topic: %s\nPayload: %s\n", MQTT_STATUS_TOPIC, statusJson.c_str());
    bool success = mqttClient.publish(MQTT_STATUS_TOPIC, statusJson.c_str(), false);
    if (success) {
        Serial.println("Device status published to MQTT");
    } else {
        Serial.println("Failed to publish device status to MQTT");
    }
}

void publishAvailability(bool online) {
    if (mqttClient.connected()) {
        const char* status = online ? "online" : "offline";
        telnetPrintf("[MQTT] Publishing availability to topic: %s\nPayload: %s\n", MQTT_AVAILABILITY_TOPIC, status);
        mqttClient.publish(MQTT_AVAILABILITY_TOPIC, status, true);
        Serial.printf("MQTT availability: %s\n", status);
    }
}

String getDeviceStatusJSON() {
    JsonDocument statusDoc;
    
    // Basic device info
    statusDoc["device_name"] = deviceName;
    statusDoc["firmware_version"] = firmwareVersion;
    statusDoc["status"] = "online";
    
    // Network info
    statusDoc["ip_address"] = WiFi.localIP().toString();
    statusDoc["wifi_signal_dbm"] = WiFi.RSSI();
    statusDoc["wifi_signal_percentage"] = constrain(2 * (WiFi.RSSI() + 100), 0, 100);
    statusDoc["wifi_quality"] = classifyWiFiSignal(WiFi.RSSI());
    
    // System info
    statusDoc["uptime_ms"] = millis();
    statusDoc["uptime_formatted"] = formatUptime(millis());
    statusDoc["free_memory_kb"] = ESP.getFreeHeap() / 1024;
    statusDoc["total_memory_kb"] = ESP.getHeapSize() / 1024;
    statusDoc["free_memory_formatted"] = getMemoryUsage();
    // Add free_memory_percent
    if (ESP.getHeapSize() > 0) {
        statusDoc["free_memory_percent"] = (ESP.getFreeHeap() * 100) / ESP.getHeapSize();
    } else {
        statusDoc["free_memory_percent"] = 0;
    }
    statusDoc["firmware_version"] = firmwareVersion;
    // Alerts
    extern bool areAlertsPaused();
    statusDoc["alerts_paused"] = areAlertsPaused();
    
    // DNS status (using external variables from dns_manager.h)
    extern bool isDNSWorking;
    extern unsigned long lastDNSCheck;
    extern unsigned long dnsFailureStartTime;
    
    statusDoc["dns_working"] = isDNSWorking;
    statusDoc["last_dns_check"] = lastDNSCheck;
    if (!isDNSWorking && dnsFailureStartTime > 0) {
        statusDoc["dns_down_duration_ms"] = millis() - dnsFailureStartTime;
    }
    
    // Heartbeat info (using external variables)
    extern unsigned long lastSuccessfulHeartbeat;
    extern int lastHeartbeatResponseCode;

    statusDoc["last_heartbeat_uptime_ms"] = lastSuccessfulHeartbeat;
    if (lastSuccessfulHeartbeat > 0) {
        statusDoc["last_heartbeat_code"] = lastHeartbeatResponseCode;
        statusDoc["last_heartbeat_formatted"] = formatUptime(lastSuccessfulHeartbeat);
        statusDoc["time_since_last_success_seconds"] = (millis() - lastSuccessfulHeartbeat) / 1000;
    } else {
        statusDoc["last_heartbeat_formatted"] = "Never";
    }
    
    // DNS configuration
    statusDoc["primary_dns"] = primaryDNS.toString();
    statusDoc["fallback_dns"] = fallbackDNS.toString();
    statusDoc["current_dns1"] = WiFi.dnsIP(0).toString();
    statusDoc["current_dns2"] = WiFi.dnsIP(1).toString();
    
    // Timestamp
    statusDoc["timestamp"] = millis();
    
    String jsonString;
    serializeJson(statusDoc, jsonString);
    return jsonString;
}

void handleMQTTLoop() {
    if (!mqttClient.connected()) {
        connectToMQTT();
        return;
    }
    
    mqttClient.loop();
    
    // Publish status and free memory periodically
    unsigned long now = millis();
    if (now - lastStatusPublish >= STATUS_PUBLISH_INTERVAL) {
        publishDeviceStatus();
        mqttClient.publish("homeassistant/sensor/poop_monitor/memory", getMemoryUsage().c_str(), false);
        publishMetricsIndividual();
        lastStatusPublish = now;
    }
}

bool isMQTTConnected() {
    return mqttClient.connected();
}

void publishSwitch(const char* object_id, const char* name, const char* command_topic, 
                   const char* state_topic, const char* icon) {
    
    // Build discovery topic
    String discoveryTopic = String(MQTT_DISCOVERY_PREFIX) + "/switch/" + 
                           HA_DEVICE_ID + "/" + object_id + "/config";
    
    // Build configuration JSON
    JsonDocument configDoc;
    configDoc["name"] = name;
    configDoc["unique_id"] = String(HA_DEVICE_ID) + "_" + object_id;
    configDoc["command_topic"] = command_topic;
    configDoc["state_topic"] = state_topic;
    configDoc["value_template"] = "{{ 'OFF' if value_json.alerts_paused else 'ON' }}";
    configDoc["payload_on"] = "ON";
    configDoc["payload_off"] = "OFF";
    configDoc["availability_topic"] = MQTT_AVAILABILITY_TOPIC;
    
    if (icon) {
        configDoc["icon"] = icon;
    }
    
    // Add device information
    configDoc["device"]["identifiers"][0] = HA_DEVICE_ID;
    configDoc["device"]["name"] = HA_DEVICE_NAME;
    configDoc["device"]["manufacturer"] = HA_MANUFACTURER;
    configDoc["device"]["model"] = HA_MODEL;
    configDoc["device"]["sw_version"] = firmwareVersion;
    
    // Serialize and publish
    String configJson;
    configJson.reserve(384);
    serializeJson(configDoc, configJson);
    
    bool success = mqttClient.publish(discoveryTopic.c_str(), configJson.c_str(), true);
    Serial.printf("Discovery %s: %s (%s)\n", 
                  success ? "OK" : "FAILED", 
                  object_id, 
                  discoveryTopic.c_str());
}

void publishButton(const char* object_id, const char* name, const char* command_topic, 
                   const char* icon) {
    
    // Build discovery topic
    String discoveryTopic = String(MQTT_DISCOVERY_PREFIX) + "/button/" + 
                           HA_DEVICE_ID + "/" + object_id + "/config";
    
    // Build configuration JSON
    JsonDocument configDoc;
    configDoc["name"] = name;
    configDoc["unique_id"] = String(HA_DEVICE_ID) + "_" + object_id;
    configDoc["command_topic"] = command_topic;
    configDoc["availability_topic"] = MQTT_AVAILABILITY_TOPIC;
    
    if (icon) {
        configDoc["icon"] = icon;
    }
    
    // Add device information
    configDoc["device"]["identifiers"][0] = HA_DEVICE_ID;
    configDoc["device"]["name"] = HA_DEVICE_NAME;
    configDoc["device"]["manufacturer"] = HA_MANUFACTURER;
    configDoc["device"]["model"] = HA_MODEL;
    configDoc["device"]["sw_version"] = firmwareVersion;
    
    // Serialize and publish
    String configJson;
    serializeJson(configDoc, configJson);
    
    bool success = mqttClient.publish(discoveryTopic.c_str(), configJson.c_str(), true);
    Serial.printf("Discovery %s: %s (%s)\n", 
                  success ? "OK" : "FAILED", 
                  object_id, 
                  discoveryTopic.c_str());
}

void publishTelnetLog(const String& logMessage) {
    if (!mqttClient.connected()) {
        return;
    }
    
    // Publish telnet log message to Home Assistant
    // Avoid echoing telnet log publish back into telnet buffer to prevent recursion/amplification
    bool success = mqttClient.publish(MQTT_TELNET_TOPIC, logMessage.c_str(), false);
    if (!success) {
        telnetPrintf("Failed to publish telnet log to MQTT\n");
    }
}

void onMQTTMessage(char* topic, byte* payload, unsigned int length) {
    // Convert payload to string
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    String topicStr = String(topic);
    Serial.printf("MQTT message received: %s -> %s\n", topic, message.c_str());
    
    // Handle reboot command
    if (topicStr == String(MQTT_COMMAND_TOPIC) + "/reboot") {
        Serial.println("MQTT reboot command received");
        publishAvailability(false);
        delay(100);  // Give time for MQTT message to send
        // Use system utils reboot function
        extern void setRebootFlag(const char* reason);
        setRebootFlag("MQTT reboot command");
    }
    // Handle alert control
    else if (topicStr == String(MQTT_COMMAND_TOPIC) + "/alerts") {
        if (message == "ON") {
            Serial.println("MQTT alerts enable command received");
            extern void resumeAlerts();
            resumeAlerts();
        } else if (message == "OFF") {
            Serial.println("MQTT alerts disable command received");
            extern void pauseAlertsIndefinitely();
            pauseAlertsIndefinitely();
        }
        // Publish updated alerts state
        delay(100);
        publishAllSensors();
    }
}

#endif // ENABLE_MQTT
