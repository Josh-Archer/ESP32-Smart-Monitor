#include "web_server.h"
#include "config.h"
#include "telnet.h"
#include "system_utils.h"
#include "dns_manager.h"
#include <WebServer.h>

WebServer server(80);

// Telnet log streaming variables
String telnetLogBuffer = "";
bool telnetStreamActive = false;
const size_t MAX_LOG_BUFFER_SIZE = 8192; // 8KB buffer

// External variables for tracking heartbeat status
extern unsigned long lastSuccessfulHeartbeat;
extern int lastHeartbeatResponseCode;

// CORS helper
static void addCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, HEAD, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void handleOptions() {
  addCORS();
  server.send(204);
}

void handleRoot() {
  // Minimal HTML landing page (UI is hosted externally)
  String html = "<html><head><title>" + String(deviceName) + "</title></head>";
  html += "<body><h1>" + String(deviceName) + "</h1>";
  html += "<p><a href='/status'>Status JSON</a> | <a href='/reboot'>Reboot</a></p>";
  html += "<script>fetch('/status').then(r=>r.json()).then(d=>document.body.innerHTML+='<p>Version: '+d.version+'</p>');</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleReboot() {
  // Simple confirmation page
  addCORS();
  server.send(200, "text/html", 
    "<html><head><meta http-equiv='refresh' content='10;url=/'></head>"
    "<body><h1>Rebooting...</h1><p>Page will refresh in 10 seconds.</p></body></html>");
  
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
  
  addCORS();
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
    addCORS();
    server.send(400, "text/plain", "Invalid pause duration");
    return;
  }
  
  telnetPrintf("[%10lu ms] [WEB] Alert pause requested via web interface\r\n", millis());
  
  // Send JSON response for API call
  String json = "{\"status\":\"success\",\"message\":\"Alerts paused\"}";
  addCORS();
  server.send(200, "application/json", json);
}

void handleAlertResume() {
  resumeAlerts();
  
  telnetPrintf("[%10lu ms] [WEB] Alert resume requested via web interface\r\n", millis());
  
  // Send JSON response for API call
  String json = "{\"status\":\"success\",\"message\":\"Alerts resumed\"}";
  addCORS();
  server.send(200, "application/json", json);
}

void handleTelnetStart() {
  telnetStreamActive = true;
  telnetLogBuffer = ""; // Clear existing buffer
  
  String json = "{\"status\":\"started\",\"message\":\"Telnet log streaming started\"}";
  addCORS();
  server.send(200, "application/json", json);
  
  telnetPrintf("[%10lu ms] [WEB] Telnet log streaming started\r\n", millis());
}

void handleTelnetStop() {
  telnetStreamActive = false;
  
  String json = "{\"status\":\"stopped\",\"message\":\"Telnet log streaming stopped\"}";
  addCORS();
  server.send(200, "application/json", json);
  
  telnetPrintf("[%10lu ms] [WEB] Telnet log streaming stopped\r\n", millis());
}

void handleTelnetOutput() {
  String json = "{";
  json += "\"output\":\"" + escapeJsonString(telnetLogBuffer) + "\",";
  json += "\"timestamp\":" + String(millis()) + ",";
  json += "\"active\":" + String(telnetStreamActive ? "true" : "false");
  json += "}";
  
  addCORS();
  server.send(200, "application/json", json);
  
  // Clear buffer after sending
  telnetLogBuffer = "";
}

// Helper function to escape strings for JSON
String escapeJsonString(const String& input) {
  String result;
  result.reserve(input.length() + 20); // Reserve some extra space
  
  for (size_t i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    switch (c) {
      case '"':  result += "\\\""; break;
      case '\\': result += "\\\\"; break;
      case '\b': result += "\\b"; break;
      case '\f': result += "\\f"; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      default:
        if (c < 0x20) {
          result += "\\u";
          result += String(c, HEX);
        } else {
          result += c;
        }
        break;
    }
  }
  return result;
}

// Function to add log entry to buffer (called from telnet module)
void addToTelnetLogBuffer(const String& logEntry) {
  if (!telnetStreamActive) return;
  
  // Add timestamp and log entry
  telnetLogBuffer += logEntry + "\n";
  
  // Trim buffer if it gets too large
  if (telnetLogBuffer.length() > MAX_LOG_BUFFER_SIZE) {
    // Keep only the last 75% of the buffer
    size_t keepSize = MAX_LOG_BUFFER_SIZE * 3 / 4;
    size_t removeSize = telnetLogBuffer.length() - keepSize;
    telnetLogBuffer.remove(0, removeSize);
  }
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: " + server.uri() + "\n";
  message += "Method: " + String((server.method() == HTTP_GET) ? "GET" : "POST") + "\n";
  message += "Arguments: " + String(server.args()) + "\n";
  
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  
  addCORS();
  server.send(404, "text/plain", message);
}

void initWebServer() {
  // UI is hosted externally; no SPIFFS mount required
  
  // Main routes
  server.on("/", handleRoot);
  server.on("/reboot", handleReboot);
  server.on("/status", handleStatus);
  server.on("/status", HTTP_HEAD, [](){ addCORS(); server.send(200); });
  
  // Alert control routes
  server.on("/alerts/pause/30", handleAlertPause);
  server.on("/alerts/pause/60", handleAlertPause);
  server.on("/alerts/pause/180", handleAlertPause);
  server.on("/alerts/pause/indefinite", handleAlertPause);
  server.on("/alerts/resume", handleAlertResume);
  
  // Telnet streaming routes
  server.on("/telnet/start", handleTelnetStart);
  server.on("/telnet/stop", handleTelnetStop);
  server.on("/telnet/output", handleTelnetOutput);
  
  // Preflight handlers
  server.on("/status", HTTP_OPTIONS, handleOptions);
  server.on("/alerts/pause/30", HTTP_OPTIONS, handleOptions);
  server.on("/alerts/pause/60", HTTP_OPTIONS, handleOptions);
  server.on("/alerts/pause/180", HTTP_OPTIONS, handleOptions);
  server.on("/alerts/pause/indefinite", HTTP_OPTIONS, handleOptions);
  server.on("/alerts/resume", HTTP_OPTIONS, handleOptions);
  server.on("/telnet/start", HTTP_OPTIONS, handleOptions);
  server.on("/telnet/stop", HTTP_OPTIONS, handleOptions);
  server.on("/telnet/output", HTTP_OPTIONS, handleOptions);
  server.on("/reboot", HTTP_OPTIONS, handleOptions);
  
  server.onNotFound(handleNotFound);
  
  server.begin();
  telnetPrintf("[%10lu ms] [WEB] HTTP server started on port 80\r\n", millis());
  telnetPrintf("[%10lu ms] [WEB] API endpoints ready (UI hosted externally)\r\n", millis());
  telnetPrintf("[%10lu ms] [WEB] Access via: http://%s or http://%s.local\r\n", 
               millis(), WiFi.localIP().toString().c_str(), deviceName);
}

void handleWebServer() {
  server.handleClient();
}
