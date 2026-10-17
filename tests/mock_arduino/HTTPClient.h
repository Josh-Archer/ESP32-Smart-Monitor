#pragma once
#include "Arduino.h"
#include "WiFiClientSecure.h"
#include <map>
#include <string>

struct MockHTTPClient {
  static int beginCalls;
  static int postCalls;
  static int lastConnectTimeout;
  static int lastTimeout;
  static std::string lastUrl;
  static std::string lastPostData;
  static std::map<std::string, std::string> headers;
  static int mockResponseCode;
  static std::string mockResponseBody;

  static void reset() {
    beginCalls = 0;
    postCalls = 0;
    lastConnectTimeout = 0;
    lastTimeout = 0;
    lastUrl.clear();
    lastPostData.clear();
    headers.clear();
    mockResponseCode = 200;
    mockResponseBody = "{\"status\":1}";
  }

  bool begin(WiFiClientSecure&, const char* url) {
    beginCalls++;
    lastUrl = url ? url : "";
    return true;
  }

  void setConnectTimeout(int32_t ms) {
    lastConnectTimeout = ms;
  }

  void setTimeout(uint16_t ms) {
    lastTimeout = ms;
  }

  void addHeader(const String& name, const String& value) {
    headers[name.c_str()] = value.c_str();
  }

  int POST(const String& postData) {
    postCalls++;
    lastPostData = postData.c_str();
    return mockResponseCode;
  }

  String getString() {
    return String(mockResponseBody);
  }

  String errorToString(int code) {
    return String("MockError");
  }

  void end() {}
};

inline int MockHTTPClient::beginCalls = 0;
inline int MockHTTPClient::postCalls = 0;
inline int MockHTTPClient::lastConnectTimeout = 0;
inline int MockHTTPClient::lastTimeout = 0;
inline std::string MockHTTPClient::lastUrl = "";
inline std::string MockHTTPClient::lastPostData = "";
inline std::map<std::string, std::string> MockHTTPClient::headers;
inline int MockHTTPClient::mockResponseCode = 200;
inline std::string MockHTTPClient::mockResponseBody = "{\"status\":1}";

using HTTPClient = MockHTTPClient;
