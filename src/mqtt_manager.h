#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#ifdef ENABLE_MQTT

#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// MQTT Topics
extern const char* MQTT_DEVICE_TOPIC;
extern const char* MQTT_STATUS_TOPIC;
extern const char* MQTT_AVAILABILITY_TOPIC;
extern const char* MQTT_DISCOVERY_PREFIX;
extern const char* MQTT_TELNET_TOPIC;
extern const char* MQTT_COMMAND_TOPIC;

// Home Assistant Device Info
extern const char* HA_DEVICE_NAME;
extern const char* HA_DEVICE_ID;
extern const char* HA_MANUFACTURER;
extern const char* HA_MODEL;

// MQTT connection and publishing
void initializeMQTT();
void connectToMQTT();
void publishHomeAssistantDiscovery();
void publishDeviceStatus();
void publishAvailability(bool online = true);
void publishTelnetLog(const String& logMessage);
void handleMQTTLoop();
bool isMQTTConnected();

// MQTT command handling
void onMQTTMessage(char* topic, byte* payload, unsigned int length);

// Helper functions
String getDeviceStatusJSON();
void publishSensor(const char* component, const char* object_id, const char* name, 
                   const char* unit_of_measurement, const char* device_class, 
                   const char* state_topic, const char* icon = nullptr);
void publishSwitch(const char* object_id, const char* name, const char* command_topic, 
                   const char* state_topic, const char* icon = nullptr);
void publishButton(const char* object_id, const char* name, const char* command_topic, 
                   const char* icon = nullptr);

#else

// Stub functions when MQTT is disabled
inline void initializeMQTT() {}
inline void connectToMQTT() {}
inline void publishHomeAssistantDiscovery() {}
inline void publishDeviceStatus() {}
inline void publishAvailability(bool online = true) {}
inline void publishTelnetLog(const String& logMessage) {}
inline void handleMQTTLoop() {}
inline bool isMQTTConnected() { return false; }
inline void onMQTTMessage(char* topic, byte* payload, unsigned int length) {}
inline String getDeviceStatusJSON() { return "{}"; }

#endif // ENABLE_MQTT

#endif // MQTT_MANAGER_H
