#include "Arduino.h"
#include "WiFi.h"
#include "WiFiClientSecure.h"
#include "HTTPClient.h"
#include "notifications.h"
#include "ota_policy.h"

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
  MockWiFiClientSecure::connectCalls = 0;

  sendPushoverAlert("Test Title", "Test Message", 0);

  assert(MockHTTPClient::beginCalls == 1);
  assert(MockHTTPClient::postCalls == 1);
  assert(MockHTTPClient::lastConnectTimeout == 3000);
  assert(MockHTTPClient::lastTimeout == 3000);
  assert(MockWiFiClientSecure::instancesCreated > 0);
  assert(MockWiFi::lastDnsTimeout == 3000);
  assert(MockWiFiClientSecure::connectCalls == 1);
  assert(MockWiFiClientSecure::lastConnectHost == "api.pushover.net");
  assert(MockWiFiClientSecure::lastConnectIp == IPAddress(1, 2, 3, 4));
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
  assert(!otaPasswordConfigured(""));
  assert(!otaPasswordConfigured(nullptr));
  assert(!otaPasswordMatches("", ""));
  assert(!otaPasswordMatches("", "somepass"));
  assert(!otaPasswordMatches(nullptr, "somepass"));
  assert(!otaPasswordMatches("mypassword", ""));
  assert(!otaPasswordMatches("mypassword", "wrongpassword"));
  assert(otaPasswordMatches("mypassword", "mypassword"));
}

void test_auth_failure_rate_limiting_and_priority() {
  assert(OTA_AUTH_ALERT_PRIORITY == 0);
  unsigned long lastAuthMs = 0;

  // First auth failure at t = 1000
  bool dispatched1 = otaAuthAlertDue(1000, lastAuthMs);
  assert(dispatched1 == true);

  // Second auth failure at t = 5000 (within 60s) -> must be rate-limited
  bool dispatched2 = otaAuthAlertDue(5000, lastAuthMs);
  assert(dispatched2 == false);

  // Third auth failure at t = 61000 (after 60s) -> dispatched
  bool dispatched3 = otaAuthAlertDue(61000, lastAuthMs);
  assert(dispatched3 == true);
}

void test_filesystem_ota_requires_signing_refusal() {
  assert(!otaFilesystemNeedsSignatureRejection(false, false));
  assert(otaFilesystemNeedsSignatureRejection(false, true));
  assert(!otaFilesystemNeedsSignatureRejection(true, true));
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
  if (target == "all" || target == "test_filesystem_ota_requires_signing_refusal") {
    test_filesystem_ota_requires_signing_refusal();
    std::cout << "PASS: test_filesystem_ota_requires_signing_refusal" << std::endl;
  }

  std::cout << "\nAll host behavioural tests PASSED!" << std::endl;
  return 0;
}
