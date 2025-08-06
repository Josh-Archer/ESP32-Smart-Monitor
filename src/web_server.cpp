#include "web_server.h"
#include "config.h"
#include "telnet.h"
#include "system_utils.h"
#include "dns_manager.h"
#include <WebServer.h>

WebServer server(80);

// External variables for tracking heartbeat status
extern unsigned long lastSuccessfulHeartbeat;
extern int lastHeartbeatResponseCode;

void handleRoot() {
  String html = "<html><head><title>" + String(deviceName) + " Control Panel</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 40px; }";
  html += ".button { display: inline-block; padding: 10px 15px; margin: 5px; text-decoration: none; border-radius: 5px; color: white; }";
  html += ".button-red { background-color: #ff4444; }";
  html += ".button-blue { background-color: #4444ff; }";
  html += ".button-green { background-color: #44aa44; }";
  html += ".button-orange { background-color: #ff8844; }";
  html += ".button-gray { background-color: #888888; }";
  html += ".alert-status { padding: 10px; margin: 10px 0; border-radius: 5px; }";
  html += ".alert-enabled { background-color: #d4edda; border: 1px solid #c3e6cb; color: #155724; }";
  html += ".alert-paused { background-color: #fff3cd; border: 1px solid #ffeaa7; color: #856404; }";
  html += "</style></head>";
  html += "<body><h1>" + String(deviceName) + " Control Panel</h1>";
  html += "<p><strong>Device:</strong> " + String(deviceName) + "</p>";
  html += "<p><strong>Version:</strong> " + String(firmwareVersion) + "</p>";
  html += "<p><strong>IP:</strong> " + WiFi.localIP().toString() + "</p>";
  html += "<p><strong>Uptime:</strong> " + String(millis() / 1000) + " seconds</p>";
  
  // Alert status section
  html += "<hr><h2>Alert Status</h2>";
  if (areAlertsPaused()) {
    unsigned long timeRemaining = getAlertsPausedTimeRemaining();
    html += "<div class='alert-status alert-paused'>";
    html += "<strong>&#128277; Alerts are PAUSED</strong><br>";
    if (timeRemaining > 0) {
      html += "Time remaining: " + String(timeRemaining / 60) + " minutes, " + String(timeRemaining % 60) + " seconds";
    } else {
      html += "Paused indefinitely";
    }
    html += "</div>";
  } else {
    html += "<div class='alert-status alert-enabled'>";
    html += "<strong>&#128276; Alerts are ENABLED</strong>";
    html += "</div>";
  }
  
  html += "<hr><h2>Actions</h2>";
  html += "<h3>Device Control</h3>";
  html += "<a href='/reboot' onclick='return confirm(\"Are you sure you want to reboot?\")' class='button button-red'>Reboot Device</a>";
  html += "<a href='/status' class='button button-blue'>Check Status</a>";
  
  html += "<h3>Alert Control</h3>";
  if (areAlertsPaused()) {
    html += "<a href='/alerts/resume' class='button button-green'>Resume Alerts</a>";
  } else {
    html += "<a href='/alerts/pause/30' onclick='return confirm(\"Pause alerts for 30 minutes?\")' class='button button-orange'>Pause 30min</a>";
    html += "<a href='/alerts/pause/60' onclick='return confirm(\"Pause alerts for 1 hour?\")' class='button button-orange'>Pause 1hr</a>";
    html += "<a href='/alerts/pause/180' onclick='return confirm(\"Pause alerts for 3 hours?\")' class='button button-orange'>Pause 3hr</a>";
    html += "<a href='/alerts/pause/indefinite' onclick='return confirm(\"Pause alerts indefinitely?\")' class='button button-gray'>Pause Indefinitely</a>";
  }
  
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
  json += "\"current_uptime_formatted\":\"" + formatUptime(millis()) + "\",";
  
  // Alert status information
  json += "\"alerts_paused\":" + String(areAlertsPaused() ? "true" : "false") + ",";
  unsigned long timeRemaining = getAlertsPausedTimeRemaining();
  json += "\"alerts_paused_time_remaining_seconds\":" + String(timeRemaining);
  
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleAlertPause() {
  String path = server.uri();
  
  if (path == "/alerts/pause/30") {
    pauseAlertsForMinutes(30);
  } else if (path == "/alerts/pause/60") {
    pauseAlertsForMinutes(60);
  } else if (path == "/alerts/pause/180") {
    pauseAlertsForMinutes(180);
  } else if (path == "/alerts/pause/indefinite") {
    pauseAlertsIndefinitely();
  } else {
    server.send(400, "text/plain", "Invalid pause duration");
    return;
  }
  
  telnetPrintf("[%10lu ms] [WEB] Alert pause requested via web interface\r\n", millis());
  
  // Redirect back to main page
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handleAlertResume() {
  resumeAlerts();
  
  telnetPrintf("[%10lu ms] [WEB] Alert resume requested via web interface\r\n", millis());
  
  // Redirect back to main page
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
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
  server.on("/alerts/pause/30", handleAlertPause);
  server.on("/alerts/pause/60", handleAlertPause);
  server.on("/alerts/pause/180", handleAlertPause);
  server.on("/alerts/pause/indefinite", handleAlertPause);
  server.on("/alerts/resume", handleAlertResume);
  server.onNotFound(handleNotFound);
  
  server.begin();
  telnetPrintf("[%10lu ms] [WEB] HTTP server started on port 80\r\n", millis());
  telnetPrintf("[%10lu ms] [WEB] Access via: http://%s or http://%s.local\r\n", 
               millis(), WiFi.localIP().toString().c_str(), deviceName);
}

void handleWebServer() {
  server.handleClient();
}
