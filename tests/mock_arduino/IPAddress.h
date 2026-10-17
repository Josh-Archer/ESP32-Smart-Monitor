#pragma once
#include <string>
#include <cstdint>

class IPAddress {
public:
  IPAddress() = default;
  IPAddress(uint8_t, uint8_t, uint8_t, uint8_t) {}
  std::string toString() const { return "192.168.1.1"; }
};
