#include "system_utils.h"
#include "config.h"
#include "telnet.h"
#include <Preferences.h>

Preferences rebootPrefs;

void rebootDevice(unsigned long delayMs, const char* reason) {
  telnetPrintf("\r\n[%10lu ms] *** REBOOT INITIATED ***\r\n", millis());
  telnetPrintf("[%10lu ms] Reason: %s\r\n", millis(), reason);
  telnetPrintf("[%10lu ms] Device: %s | Version: %s\r\n", millis(), deviceName, firmwareVersion);
  telnetPrintf("[%10lu ms] Rebooting in %lu ms...\r\n", millis(), delayMs);
  
  // Flush any pending telnet/serial output
  delay(100);
  
  // Optional: Save reboot reason to preferences for post-reboot logging
  rebootPrefs.begin("system", false);
  rebootPrefs.putString("last_reboot", reason);
  rebootPrefs.putULong("reboot_time", millis());
  rebootPrefs.end();
  
  delay(delayMs);
  
  telnetPrintf("[%10lu ms] *** REBOOTING NOW ***\r\n", millis());
  delay(100);
  
  ESP.restart();
}

bool checkRebootFlag() {
  rebootPrefs.begin("system", true);
  bool shouldReboot = rebootPrefs.getBool("reboot_flag", false);
  rebootPrefs.end();
  
  if (shouldReboot) {
    // Clear the flag
    rebootPrefs.begin("system", false);
    rebootPrefs.remove("reboot_flag");
    rebootPrefs.end();
    return true;
  }
  
  return false;
}

void setRebootFlag(const char* reason) {
  rebootPrefs.begin("system", false);
  rebootPrefs.putBool("reboot_flag", true);
  rebootPrefs.putString("reboot_reason", reason);
  rebootPrefs.end();
  
  telnetPrintf("[%10lu ms] [SYSTEM] Reboot flag set: %s\r\n", millis(), reason);
}

String formatUptime(unsigned long uptimeMs) {
  unsigned long uptimeSeconds = uptimeMs / 1000;
  unsigned long hours = uptimeSeconds / 3600;
  unsigned long minutes = (uptimeSeconds % 3600) / 60;
  unsigned long seconds = uptimeSeconds % 60;
  
  return String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s";
}
