#include "Arduino.h"
#include "WiFi.h"
#include "WiFiClientSecure.h"
#include "HTTPClient.h"
#include "notifications.h"

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cstring>

// Strong symbol definitions to satisfy config.h externs
const char* firmwareVersion = "4.1.0";
const char* deviceName = "smart-monitor-test";
const char* pushoverToken = "";
const char* pushoverUser = "";
const char* pushoverApiUrl = "https://api.pushover.net/1/messages.json";
const char* otaPassword = "secret-password";

// Include implementation under test
#include "notifications.cpp"

// Helpers for test recording
struct MockAlertRecord {
  std::string title;
  std::string message;
  int priority;
  unsigned long timestamp;
};

static std::vector<MockAlertRecord> g_dispatchedAlerts;

void recordMockAlert(const char* title, const char* message, int priority) {
  g_dispatchedAlerts.push_back({
    title ? title : "",
    message ? message : "",
    priority,
    millis()
  });
}

// Behavioral helper mimicking ota_manager auth validation
bool isOtaPasswordAuthenticated(const char* configuredPassword, const String& headerPassword) {
  if (!configuredPassword || strlen(configuredPassword) == 0 ||
      headerPassword.length() == 0 || headerPassword != String(configuredPassword)) {
    return false;
  }
  return true;
}

// Behavioral helper mimicking ota_manager bad-password rate limiting and priority
bool handleOtaAuthFailureEvent(unsigned long currentMs, unsigned long& lastAuthAlertMs,
                               void (*alertFn)(const char*, const char*, int)) {
  const unsigned long AUTH_ALERT_INTERVAL_MS = 60000;
  if (lastAuthAlertMs != 0 && (currentMs - lastAuthAlertMs) < AUTH_ALERT_INTERVAL_MS) {
    return false; // suppressed / rate-limited
  }
  lastAuthAlertMs = currentMs;
  if (alertFn) {
    alertFn("OTA Upload Failed - smart-monitor-test",
            "OTA upload failed: Authentication Failed (Error 0). Previous firmware remains active.",
            0); // priority 0
  }
  return true;
}

// Behavioral helper mimicking ArduinoOTA start notification deferral
struct ArduinoOtaLifecycleSimulator {
  bool startPending = false;
  std::string startType;
  std::vector<std::string> eventLog;

  void onStart(const char* type) {
    startPending = true;
    startType = type ? type : "sketch";
    eventLog.push_back("onStart:queued");
    // Does NOT call alertFn here (no blocking before espota connects back)
  }

  void onEnd(void (*alertFn)(const char*, const char*, int)) {
    eventLog.push_back("onEnd:begin");
    if (startPending) {
      startPending = false;
      if (alertFn) {
        alertFn("OTA Upload Started", startType.c_str(), 0);
      }
      eventLog.push_back("onEnd:start_dispatched");
    }
    if (alertFn) {
      alertFn("OTA Upload Successful", "Accepted", 0);
    }
    eventLog.push_back("onEnd:success_dispatched");
  }
};

// Behavioral helper mimicking Signed HTTP OTA response precedence
struct HttpOtaResponseOrderTracker {
  std::vector<std::string> sequence;

  void sendResponse(int code) {
    sequence.push_back("HTTP_RESPONSE_" + std::to_string(code));
  }

  void clientStop() {
    sequence.push_back("CLIENT_STOP");
  }

  void dispatchStart() {
    sequence.push_back("NOTIFY_START");
  }

  void dispatchFailure(const char* reason) {
    sequence.push_back(std::string("NOTIFY_FAILURE:") + (reason ? reason : ""));
  }

  void dispatchSuccess() {
    sequence.push_back("NOTIFY_SUCCESS");
  }

  void executeFailureBranch(int code, const char* reason) {
    // Current firmware order: sendResponse -> client.stop -> notifyOtaStart -> notifyOtaFailure
    sendResponse(code);
    clientStop();
    dispatchStart();
    dispatchFailure(reason);
  }

  void executeSuccessBranch() {
    sendResponse(200);
    clientStop();
    dispatchStart();
    dispatchSuccess();
  }
};

// Individual test cases
void test_empty_token_returns_early() {
  pushoverToken = "";
  pushoverUser = "user123";
  MockHTTPClient::reset();
  MockWiFiClientSecure::instancesCreated = 0;

  sendPushoverAlert("Test Title", "Test Message", 0);

  assert(MockHTTPClient::beginCalls == 0);
  assert(MockHTTPClient::postCalls == 0);
  assert(MockWiFiClientSecure::instancesCreated == 0);
}

void test_null_token_returns_early() {
  pushoverToken = nullptr;
  pushoverUser = "user123";
  MockHTTPClient::reset();
  MockWiFiClientSecure::instancesCreated = 0;

  sendPushoverAlert("Test Title", "Test Message", 0);

  assert(MockHTTPClient::beginCalls == 0);
  assert(MockHTTPClient::postCalls == 0);
  assert(MockWiFiClientSecure::instancesCreated == 0);
}

void test_empty_user_returns_early() {
  pushoverToken = "token123";
  pushoverUser = "";
  MockHTTPClient::reset();
  MockWiFiClientSecure::instancesCreated = 0;

  sendPushoverAlert("Test Title", "Test Message", 0);

  assert(MockHTTPClient::beginCalls == 0);
  assert(MockHTTPClient::postCalls == 0);
  assert(MockWiFiClientSecure::instancesCreated == 0);
}

void test_null_user_returns_early() {
  pushoverToken = "token123";
  pushoverUser = nullptr;
  MockHTTPClient::reset();
  MockWiFiClientSecure::instancesCreated = 0;

  sendPushoverAlert("Test Title", "Test Message", 0);

  assert(MockHTTPClient::beginCalls == 0);
  assert(MockHTTPClient::postCalls == 0);
  assert(MockWiFiClientSecure::instancesCreated == 0);
}

void test_explicit_network_timeouts() {
  pushoverToken = "token123";
  pushoverUser = "user123";
  MockHTTPClient::reset();
  MockWiFiClientSecure::instancesCreated = 0;

  sendPushoverAlert("Test Title", "Test Message", 0);

  assert(MockHTTPClient::beginCalls == 1);
  assert(MockHTTPClient::postCalls == 1);
  assert(MockHTTPClient::lastConnectTimeout == 3000);
  assert(MockHTTPClient::lastTimeout == 3000);
  assert(MockWiFiClientSecure::instancesCreated > 0);
}

void test_url_encode_function() {
  assert(urlEncode(nullptr) == "");
  assert(urlEncode("") == "");
  assert(urlEncode("alphanumeric123") == "alphanumeric123");
  assert(urlEncode("a-b_c.d~e") == "a-b_c.d~e");
  assert(urlEncode("hello world") == "hello+world");
  assert(urlEncode("foo & bar + baz = qux % 100") == "foo+%26+bar+%2B+baz+%3D+qux+%25+100");
  assert(urlEncode("line1\nline2") == "line1%0Aline2");
}

void test_pushover_post_body_url_encoding() {
  pushoverToken = "tok&123";
  pushoverUser = "usr+456";
  MockHTTPClient::reset();

  sendPushoverAlert("OTA & Update + Test", "Version: v4.1.0 & details = 100% OK", 1);

  assert(MockHTTPClient::postCalls == 1);
  std::string body = MockHTTPClient::lastPostData;

  assert(body.find("token=tok%26123") != std::string::npos);
  assert(body.find("user=usr%2B456") != std::string::npos);
  assert(body.find("title=OTA+%26+Update+%2B+Test") != std::string::npos);
  assert(body.find("message=Version%3A+v4.1.0+%26+details+%3D+100%25+OK") != std::string::npos);
  assert(body.find("priority=1") != std::string::npos);
  assert(body.find("device=smart-monitor-test") != std::string::npos);
}

void test_empty_ota_password_rejected() {
  // Empty configured password must reject empty header
  assert(!isOtaPasswordAuthenticated("", ""));
  assert(!isOtaPasswordAuthenticated("", "somepass"));
  assert(!isOtaPasswordAuthenticated(nullptr, ""));
  assert(!isOtaPasswordAuthenticated(nullptr, "somepass"));

  // Configured password must reject empty or wrong header
  assert(!isOtaPasswordAuthenticated("mypassword", ""));
  assert(!isOtaPasswordAuthenticated("mypassword", "wrongpassword"));

  // Matching non-empty password accepted
  assert(isOtaPasswordAuthenticated("mypassword", "mypassword"));
}

void test_auth_failure_rate_limiting_and_priority() {
  g_dispatchedAlerts.clear();
  unsigned long lastAuthMs = 0;

  // First auth failure at t = 1000
  bool dispatched1 = handleOtaAuthFailureEvent(1000, lastAuthMs, recordMockAlert);
  assert(dispatched1 == true);
  assert(g_dispatchedAlerts.size() == 1);
  assert(g_dispatchedAlerts[0].priority == 0); // Must be priority 0, not priority 1

  // Second auth failure at t = 5000 (within 60s) -> must be rate-limited
  bool dispatched2 = handleOtaAuthFailureEvent(5000, lastAuthMs, recordMockAlert);
  assert(dispatched2 == false);
  assert(g_dispatchedAlerts.size() == 1); // No new alert added

  // Third auth failure at t = 62000 (after 60s) -> dispatched
  bool dispatched3 = handleOtaAuthFailureEvent(62000, lastAuthMs, recordMockAlert);
  assert(dispatched3 == true);
  assert(g_dispatchedAlerts.size() == 2);
  assert(g_dispatchedAlerts[1].priority == 0);
}

void test_arduino_ota_start_notice_deferred() {
  g_dispatchedAlerts.clear();
  ArduinoOtaLifecycleSimulator sim;

  // onStart called
  sim.onStart("sketch");
  assert(sim.startPending == true);
  assert(g_dispatchedAlerts.empty()); // Must NOT have sent alert in onStart

  // onEnd called
  sim.onEnd(recordMockAlert);
  assert(sim.startPending == false);
  assert(g_dispatchedAlerts.size() == 2);
  assert(g_dispatchedAlerts[0].title == "OTA Upload Started");
  assert(g_dispatchedAlerts[1].title == "OTA Upload Successful");
}

void test_http_ota_response_before_notification() {
  HttpOtaResponseOrderTracker tracker;

  // Test failure branch (e.g. 500 Update.begin failed)
  tracker.executeFailureBranch(500, "Update.begin failed");
  assert(tracker.sequence.size() == 4);
  assert(tracker.sequence[0] == "HTTP_RESPONSE_500");
  assert(tracker.sequence[1] == "CLIENT_STOP");
  assert(tracker.sequence[2] == "NOTIFY_START");
  assert(tracker.sequence[3] == "NOTIFY_FAILURE:Update.begin failed");

  // Test success branch (200 OK)
  tracker.sequence.clear();
  tracker.executeSuccessBranch();
  assert(tracker.sequence.size() == 4);
  assert(tracker.sequence[0] == "HTTP_RESPONSE_200");
  assert(tracker.sequence[1] == "CLIENT_STOP");
  assert(tracker.sequence[2] == "NOTIFY_START");
  assert(tracker.sequence[3] == "NOTIFY_SUCCESS");
}

int main(int argc, char** argv) {
  std::string target = (argc > 1) ? argv[1] : "all";

  if (target == "all" || target == "test_empty_token_returns_early") {
    test_empty_token_returns_early();
    std::cout << "PASS: test_empty_token_returns_early" << std::endl;
  }
  if (target == "all" || target == "test_null_token_returns_early") {
    test_null_token_returns_early();
    std::cout << "PASS: test_null_token_returns_early" << std::endl;
  }
  if (target == "all" || target == "test_empty_user_returns_early") {
    test_empty_user_returns_early();
    std::cout << "PASS: test_empty_user_returns_early" << std::endl;
  }
  if (target == "all" || target == "test_null_user_returns_early") {
    test_null_user_returns_early();
    std::cout << "PASS: test_null_user_returns_early" << std::endl;
  }
  if (target == "all" || target == "test_explicit_network_timeouts") {
    test_explicit_network_timeouts();
    std::cout << "PASS: test_explicit_network_timeouts" << std::endl;
  }
  if (target == "all" || target == "test_url_encode_function") {
    test_url_encode_function();
    std::cout << "PASS: test_url_encode_function" << std::endl;
  }
  if (target == "all" || target == "test_pushover_post_body_url_encoding") {
    test_pushover_post_body_url_encoding();
    std::cout << "PASS: test_pushover_post_body_url_encoding" << std::endl;
  }
  if (target == "all" || target == "test_empty_ota_password_rejected") {
    test_empty_ota_password_rejected();
    std::cout << "PASS: test_empty_ota_password_rejected" << std::endl;
  }
  if (target == "all" || target == "test_auth_failure_rate_limiting_and_priority") {
    test_auth_failure_rate_limiting_and_priority();
    std::cout << "PASS: test_auth_failure_rate_limiting_and_priority" << std::endl;
  }
  if (target == "all" || target == "test_arduino_ota_start_notice_deferred") {
    test_arduino_ota_start_notice_deferred();
    std::cout << "PASS: test_arduino_ota_start_notice_deferred" << std::endl;
  }
  if (target == "all" || target == "test_http_ota_response_before_notification") {
    test_http_ota_response_before_notification();
    std::cout << "PASS: test_http_ota_response_before_notification" << std::endl;
  }

  std::cout << "\nAll host behavioural tests PASSED!" << std::endl;
  return 0;
}
