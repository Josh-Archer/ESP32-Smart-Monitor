#pragma once
#include <string>
#include <cstdint>

class IPAddress {
  uint32_t value = 0;
public:
  IPAddress() = default;
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
      : value((uint32_t)a << 24 | (uint32_t)b << 16 | (uint32_t)c << 8 | d) {}
  std::string toString() const { return "192.168.1.1"; }
  bool fromString(const char*) { return false; }
  uint32_t raw() const { return value; }
};
