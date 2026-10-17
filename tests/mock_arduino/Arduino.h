#pragma once
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>

class String {
  std::string s;
public:
  String() = default;
  String(const char* str) : s(str ? str : "") {}
  String(const std::string& str) : s(str) {}
  String(int v) : s(std::to_string(v)) {}
  String(unsigned int v) : s(std::to_string(v)) {}
  String(long v) : s(std::to_string(v)) {}
  String(unsigned long v) : s(std::to_string(v)) {}

  const char* c_str() const { return s.c_str(); }
  size_t length() const { return s.length(); }
  void reserve(size_t n) { s.reserve(n); }

  String& operator+=(char c) { s += c; return *this; }
  String& operator+=(const char* str) { if (str) s += str; return *this; }
  String& operator+=(const String& rhs) { s += rhs.s; return *this; }

  bool operator==(const String& rhs) const { return s == rhs.s; }
  bool operator!=(const String& rhs) const { return s != rhs.s; }
  bool operator==(const char* rhs) const { return s == (rhs ? rhs : ""); }
  bool operator!=(const char* rhs) const { return s != (rhs ? rhs : ""); }
  bool startsWith(const char* prefix) const {
    if (!prefix) return false;
    return s.rfind(prefix, 0) == 0;
  }
  String substring(size_t start) const {
    if (start >= s.length()) return String("");
    return String(s.substr(start));
  }
  void trim() {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) { s.clear(); return; }
    auto end = s.find_last_not_of(" \t\r\n");
    s = s.substr(start, end - start + 1);
  }
};

inline String operator+(const String& lhs, const String& rhs) {
  return String(std::string(lhs.c_str()) + rhs.c_str());
}
inline String operator+(const char* lhs, const String& rhs) {
  return String(std::string(lhs ? lhs : "") + rhs.c_str());
}
inline String operator+(const String& lhs, const char* rhs) {
  return String(std::string(lhs.c_str()) + (rhs ? rhs : ""));
}

inline unsigned long g_mockMillis = 1000;
inline unsigned long millis() { return g_mockMillis; }

struct MockSerial {
  template<typename... Args>
  void printf(const char* fmt, Args... args) {}
  void println(const char* msg = "") {}
  void println(const String& msg) {}
};
inline MockSerial Serial;
