#include "web_server.h"
#include "config.h"
#include "telnet.h"
#include "system_utils.h"
#include <WebServer.h>

WebServer server(80);

// External variables for tracking heartbeat status
extern unsigned long lastSuccessfulHeartbeat;
extern int lastHeartbeatResponseCode;

void handleRoot() {
  String html = "<html><head><title>" + String(deviceName) + " Control Panel</title></head>";
  html += "<body><h1>" + String(deviceName) + " Control Panel</h1>";
  html += "<p><strong>Device:</strong> " + String(deviceName) + "</p>";
  html += "<p><strong>Version:</strong> " + String(firmwareVersion) + "</p>";
  html += "<p><strong>IP:</strong> " + WiFi.localIP().toString() + "</p>";
  html += "<p><strong>Uptime:</strong> " + String(millis() / 1000) + " seconds</p>";
  html += "<hr>";
  html += "<h2>Actions</h2>";
  html += "<p><a href='/reboot' onclick='return confirm(\"Are you sure you want to reboot?\")' style='background-color: #ff4444; color: white; padding: 10px; text-decoration: none; border-radius: 5px;'>Reboot Device</a></p>";
  html += "<p><a href='/status' style='background-color: #4444ff; color: white; padding: 10px; text-decoration: none; border-radius: 5px;'>Check Status</a></p>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleReboot() {
  String html = "<html><head><title>Rebooting " + String(deviceName) + "</title>";
  html += "<meta http-equiv='refresh' content='10;url=/'>"; // Redirect back to main page after 10 seconds
  html += "</head><body>";
  html += "<h1>Rebooting " + String(deviceName) + "</h1>";
  html += "<p>Device is rebooting now. This page will automatically refresh in 10 seconds.</p>";
  html += "<p>You can also <a href='/'>click here</a> to return to the main page.</p>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
  
  telnetPrintf("[%10lu ms] [WEB] Reboot requested via web interface\r\n", millis());
  
  // Schedule reboot after a short delay to allow the response to be sent
  setRebootFlag("Web interface reboot request");
}

void handleStatus() {
  String json = "{";
  json += "\"device\":\"" + String(deviceName) + "\",";
  json += "\"version\":\"" + String(firmwareVersion) + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"uptime\":" + String(millis()) + ",";
  json += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"wifi_connected\":" + String(WiFi.isConnected() ? "true" : "false") + ",";
  
  // DNS server information
  json += "\"primary_dns\":\"" + primaryDNS.toString() + "\",";
  json += "\"fallback_dns\":\"" + fallbackDNS.toString() + "\",";
  json += "\"secondary_fallback_dns\":\"" + secondaryFallback.toString() + "\",";
  
  // Current DNS servers in use
  json += "\"current_dns1\":\"" + WiFi.dnsIP(0).toString() + "\",";
  json += "\"current_dns2\":\"" + WiFi.dnsIP(1).toString() + "\",";
  
  // Heartbeat information
  json += "\"last_heartbeat_success\":" + String(lastSuccessfulHeartbeat) + ",";
  json += "\"last_heartbeat_code\":" + String(lastHeartbeatResponseCode) + ",";
  json += "\"heartbeat_endpoint\":\"" + String(apiEndpoint) + "\",";
  
  // Calculate time since last successful heartbeat in seconds
  unsigned long timeSinceLastSuccessMs = millis() - lastSuccessfulHeartbeat;
  unsigned long timeSinceLastSuccessSeconds = timeSinceLastSuccessMs / 1000;
  json += "\"time_since_last_success_ms\":" + String(timeSinceLastSuccessMs) + ",";
  json += "\"time_since_last_success_seconds\":" + String(timeSinceLastSuccessSeconds) + ",";
  
  // Add formatted timestamp if we have a successful heartbeat
  if (lastSuccessfulHeartbeat > 0) {
    // Calculate approximate timestamp (device boot time + heartbeat time)
    // Note: This is relative to device uptime, not absolute time
    unsigned long uptimeAtHeartbeat = lastSuccessfulHeartbeat;
    json += "\"last_heartbeat_uptime\":" + String(uptimeAtHeartbeat) + ",";
    json += "\"last_heartbeat_uptime_formatted\":\"" + formatUptime(uptimeAtHeartbeat) + "\",";
  } else {
    json += "\"last_heartbeat_uptime\":0,";
    json += "\"last_heartbeat_uptime_formatted\":\"Never\",";
  }
  
  // Current timestamp information
  json += "\"current_uptime\":" + String(millis()) + ",";
  json += "\"current_uptime_formatted\":\"" + formatUptime(millis()) + "\"";
  
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: " + server.uri() + "\n";
  message += "Method: " + String((server.method() == HTTP_GET) ? "GET" : "POST") + "\n";
  message += "Arguments: " + String(server.args()) + "\n";
  
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  
  server.send(404, "text/plain", message);
}

void initWebServer() {
  server.on("/", handleRoot);
  server.on("/reboot", handleReboot);
  server.on("/status", handleStatus);
  server.onNotFound(handleNotFound);
  
  server.begin();
  telnetPrintf("[%10lu ms] [WEB] HTTP server started on port 80\r\n", millis());
  telnetPrintf("[%10lu ms] [WEB] Access via: http://%s or http://%s.local\r\n", 
               millis(), WiFi.localIP().toString().c_str(), deviceName);
}

void handleWebServer() {
  server.handleClient();
}
