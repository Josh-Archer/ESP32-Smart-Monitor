// Native driver for setup_qr payload builders.
// PlatformIO does not compile scripts/. Python feeds tab-separated cases on stdin.
//
// WIFI <ssid> <password> <auth> <hidden>
// DASH <url>
// HA <name> <url> <host> <port> <discovery> <device>

#include "setup_qr.h"

#include <iostream>
#include <string>
#include <vector>

static std::vector<std::string> split_tab(const std::string& line) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : line) {
    if (c == '\t') {
      out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  out.push_back(cur);
  return out;
}

static int self_check() {
  char out[256];
  int rc = setup_qr_build_wifi("Open", nullptr, "WPA", 0, out, sizeof(out));
  if (rc != SETUP_QR_OK || std::string(out) != "WIFI:T:nopass;S:Open;H:false;;") {
    std::cerr << "null password should force nopass\n";
    return 1;
  }
  rc = setup_qr_build_wifi("Open", "secret", "nopass", 0, out, sizeof(out));
  if (rc != SETUP_QR_OK || std::string(out) != "WIFI:T:nopass;S:Open;H:false;;") {
    std::cerr << "nopass must drop the password\n";
    return 1;
  }
  if (std::string(out).find("secret") != std::string::npos) {
    std::cerr << "nopass payload leaked the password\n";
    return 1;
  }
  rc = setup_qr_build_wifi("bad\nssid", "secret", "WPA", 0, out, sizeof(out));
  if (rc != SETUP_QR_ERR_INVALID) {
    std::cerr << "newline in ssid should be rejected\n";
    return 1;
  }
  rc = setup_qr_build_wifi("Cafe", "secret", "WPA", 0, out, 8);
  if (rc != SETUP_QR_ERR_BUFFER) {
    std::cerr << "short buffer should fail\n";
    return 1;
  }
  rc = setup_qr_build_ha("n", "http://192.0.2.10/", "192.0.2.20", 1883, "homeassistant", "dev", out, 8);
  if (rc != SETUP_QR_ERR_BUFFER) {
    std::cerr << "short HA buffer should fail\n";
    return 1;
  }
  return 0;
}

int main() {
  if (self_check() != 0) {
    return 2;
  }
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty()) {
      continue;
    }
    std::vector<std::string> f = split_tab(line);
    char out[1024];
    int rc = SETUP_QR_ERR_INVALID;
    if (f.size() == 5 && f[0] == "WIFI") {
      int hidden = std::stoi(f[4]);
      rc = setup_qr_build_wifi(f[1].c_str(), f[2].c_str(), f[3].c_str(), hidden, out, sizeof(out));
    } else if (f.size() == 2 && f[0] == "DASH") {
      rc = setup_qr_build_dashboard(f[1].c_str(), out, sizeof(out));
    } else if (f.size() == 7 && f[0] == "HA") {
      int port = std::stoi(f[4]);
      rc = setup_qr_build_ha(f[1].c_str(), f[2].c_str(), f[3].c_str(), port, f[5].c_str(), f[6].c_str(),
                             out, sizeof(out));
    } else {
      std::cout << "ERR\t-1\n";
      continue;
    }
    if (rc == SETUP_QR_OK) {
      std::cout << "OK\t" << out << "\n";
    } else {
      std::cout << "ERR\t" << rc << "\n";
    }
  }
  return 0;
}
