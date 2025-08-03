#include "dns_manager.h"
#include "config.h"
#include "notifications.h"
#include <WiFi.h>
#include <HTTPClient.h>

bool dnsFailureReported = false;  // Prevent spam notifications

bool testDNSResolution() {
  Serial.printf("[%10lu ms] [DNS] Testing DNS resolution...\r\n", millis());
  
  WiFiClient client;
  HTTPClient http;
  
  // Test with a reliable external service
  http.begin(client, "http://httpbin.org/ip");
  http.setTimeout(5000); // 5 second timeout
  
  int httpCode = http.GET();
  http.end();
  
  bool dnsWorking = (httpCode > 0);
  
  if (dnsWorking) {
    Serial.printf("[%10lu ms] [DNS] DNS resolution working (Primary: %s)\r\n", millis(), primaryDNS.toString().c_str());
    if (dnsFailureReported) {
      // DNS is working again, send recovery notification
      String recoveryMsg = "DNS server " + primaryDNS.toString() + " is working again on " + String(deviceName);
      sendPushoverAlert("DNS Recovered", recoveryMsg.c_str(), 0);
      dnsFailureReported = false;
    }
  } else {
    Serial.printf("[%10lu ms] [DNS] DNS resolution failed with primary DNS (%s)\r\n", millis(), primaryDNS.toString().c_str());
    
    // Try switching to fallback DNS
    Serial.printf("[%10lu ms] [DNS] Switching to fallback DNS (%s, %s)\r\n", millis(), fallbackDNS.toString().c_str(), secondaryFallback.toString().c_str());
    WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), fallbackDNS, secondaryFallback);
    
    delay(2000); // Give time for DNS change to take effect
    
    // Test again with fallback DNS
    http.begin(client, "http://httpbin.org/ip");
    http.setTimeout(5000);
    int fallbackCode = http.GET();
    http.end();
    
    if (fallbackCode > 0) {
      Serial.printf("[%10lu ms] [DNS] Fallback DNS working\r\n", millis());
      
      // Send alert about primary DNS failure but fallback working
      if (!dnsFailureReported) {
        String alertMsg = "Primary DNS " + primaryDNS.toString() + " is down on " + String(deviceName) + ". Using fallback DNS.";
        sendPushoverAlert("DNS Server Down", alertMsg.c_str(), 1);
        dnsFailureReported = true;
      }
      
      dnsWorking = true;
    } else {
      Serial.printf("[%10lu ms] [DNS] Both primary and fallback DNS failed!\r\n", millis());
      
      // Send critical alert - both DNS servers down
      if (!dnsFailureReported) {
        String criticalMsg = "Both primary (" + primaryDNS.toString() + ") and fallback DNS failed on " + String(deviceName);
        sendPushoverAlert("Critical: All DNS Down", criticalMsg.c_str(), 2);
        dnsFailureReported = true;
      }
    }
  }
  
  return dnsWorking;
}
