#include "ota_manager.h"
#include "config.h"
#include "telnet.h"
#include "notifications.h"
#include "ota_crypto.h"
#include "ota_signing_config.h"

#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <string.h>

#include "mbedtls/version.h"
#include "mbedtls/sha256.h"
#include "mbedtls/aes.h"

Preferences otaPrefs;

// Track OTA stream size so we can verify the appended signature trailer.
static size_t g_otaStreamTotal = 0;
static bool g_otaSignatureChecked = false;
static bool g_otaSignatureOk = false;

// HTTP signed (optionally encrypted) OTA on a dedicated port so MQTT-only
// builds still accept signed packages without enabling the full web UI.
static const uint16_t SIGNED_OTA_PORT = 8267;
static WiFiServer signedOtaServer(SIGNED_OTA_PORT);
static bool signedOtaServerStarted = false;

const char* getOtaSigningStatus() {
  return otaSigningStatusString();
}

static void logOta(const char* msg) {
  Serial.printf("[%10lu ms] [OTA] %s\r\n", millis(), msg);
  telnetPrintf("[%10lu ms] [OTA] %s\r\n", millis(), msg);
}

// After ArduinoOTA/Update sets the new boot partition, verify the appended
// ECDSA signature. On failure, restore the running partition so the previous
// firmware remains bootable (no reboot into untrusted image).
static bool finalizeSignedArduinoOta() {
  g_otaSignatureChecked = true;
  g_otaSignatureOk = false;

  if (!otaSignatureRequired() && !otaPublicKeyConfigured()) {
    // Legacy password-only mode — accept image as-is.
    g_otaSignatureOk = true;
    return true;
  }

  if (!otaPublicKeyConfigured()) {
    logOta("Signature enforcement enabled but public key is not provisioned — rejecting OTA");
    otaRestoreRunningBootPartition();
    return false;
  }

  if (g_otaStreamTotal <= OTA_ECDSA_SIG_LEN) {
    logOta("Signed image too small (missing signature trailer) — rejecting OTA");
    otaRestoreRunningBootPartition();
    return false;
  }

  // After Update.end(), boot partition points at the new image.
  const esp_partition_t* part = esp_ota_get_boot_partition();
  if (!part) {
    logOta("Cannot locate OTA boot partition for signature check");
    otaRestoreRunningBootPartition();
    return false;
  }

  size_t imageSize = 0;
  if (!otaVerifyPartitionSignature(part, g_otaStreamTotal, &imageSize)) {
    logOta("*** SIGNATURE VERIFICATION FAILED — keeping previous firmware ***");
    otaRestoreRunningBootPartition();
    return false;
  }

  g_otaSignatureOk = true;
  logOta("Signature verified — applying update and rebooting");
  return true;
}

void initOTA() {
  // Initialize mDNS
  if (!MDNS.begin(deviceName)) {
    Serial.printf("[%10lu ms] Error setting up MDNS responder!\r\n", millis());
  } else {
    Serial.printf("[%10lu ms] mDNS responder started. Device: %s.local\r\n", millis(), deviceName);
  }

  // Configure ArduinoOTA (espota). Password auth remains the first gate;
  // ECDSA verification (when configured) runs before reboot.
  ArduinoOTA.setHostname(deviceName);
  ArduinoOTA.setPassword(otaPassword);
  // We decide whether to reboot after signature verification.
  ArduinoOTA.setRebootOnSuccess(false);

  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    g_otaStreamTotal = 0;
    g_otaSignatureChecked = false;
    g_otaSignatureOk = false;
    Serial.printf("\r\n[%10lu ms] [OTA] *** UPDATE INITIATED ***\r\n", millis());
    Serial.printf("[%10lu ms] [OTA] Current version: %s\r\n", millis(), firmwareVersion);
    Serial.printf("[%10lu ms] [OTA] Updating %s...\r\n", millis(), type.c_str());
    Serial.printf("[%10lu ms] [OTA] Signing mode: %s\r\n", millis(), otaSigningStatusString());
    telnetPrintf("[%10lu ms] [OTA] Update initiated (%s), signing=%s\r\n",
                 millis(), type.c_str(), otaSigningStatusString());
  });

  ArduinoOTA.onEnd([]() {
    Serial.printf("\r\n[%10lu ms] [OTA] *** DOWNLOAD COMPLETE — VERIFYING ***\r\n", millis());
    telnetPrintf("[%10lu ms] [OTA] Download complete — verifying signature\r\n", millis());

    // Filesystem (SPIFFS) updates are not ECDSA-signed the same way; only
    // flash/sketch images require signature verification.
    if (ArduinoOTA.getCommand() != U_FLASH) {
      logOta("Filesystem OTA complete — rebooting");
      delay(1500);
      ESP.restart();
      return;
    }

    if (finalizeSignedArduinoOta()) {
      Serial.printf("[%10lu ms] [OTA] *** UPDATE ACCEPTED ***\r\n", millis());
      Serial.printf("[%10lu ms] [OTA] Device will restart in 2 seconds...\r\n", millis());
      delay(2000);
      ESP.restart();
    } else {
      Serial.printf("[%10lu ms] [OTA] *** UPDATE REJECTED — previous firmware bootable ***\r\n", millis());
      telnetPrintf("[%10lu ms] [OTA] UPDATE REJECTED — previous firmware remains bootable\r\n", millis());
      // Do not reboot into the new image.
    }
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    g_otaStreamTotal = total;
    static unsigned long lastReport = 0;
    unsigned long now = millis();
    if (now - lastReport > 2000) {
      unsigned int percent = total ? (progress / (total / 100)) : 0;
      Serial.printf("[%10lu ms] [OTA] Progress: %u%% (%u/%u bytes)\r\n", now, percent, progress, total);
      lastReport = now;
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("\r\n[%10lu ms] [OTA] *** UPDATE FAILED ***\r\n", millis());
    Serial.printf("[%10lu ms] [OTA] Error[%u]: ", millis(), error);
    if (error == OTA_AUTH_ERROR) Serial.println("Authentication Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    // Ensure we never leave a half-applied image as the boot target.
    otaRestoreRunningBootPartition();
  });

  ArduinoOTA.begin();
  Serial.printf("[%10lu ms] [OTA] Ready! Device: %s.local\r\n", millis(), deviceName);
  Serial.printf("[%10lu ms] [OTA] Version: %s\r\n", millis(), firmwareVersion);
  Serial.printf("[%10lu ms] [OTA] Signing: %s\r\n", millis(), otaSigningStatusString());

  // Always-on signed HTTP OTA endpoint (password + ECDSA, optional AES).
  signedOtaServer.begin();
  signedOtaServerStarted = true;
  Serial.printf("[%10lu ms] [OTA] Signed HTTP OTA listening on port %u\r\n\r\n",
                millis(), SIGNED_OTA_PORT);
  MDNS.addService("ota-signed", "tcp", SIGNED_OTA_PORT);
}

// Read exactly `len` bytes from client into buf (with timeout).
static bool readExact(WiFiClient& client, uint8_t* buf, size_t len, unsigned long timeoutMs = 120000) {
  size_t got = 0;
  unsigned long start = millis();
  while (got < len && client.connected() && (millis() - start) < timeoutMs) {
    if (client.available()) {
      int n = client.read(buf + got, len - got);
      if (n > 0) {
        got += (size_t)n;
      }
    } else {
      delay(1);
    }
  }
  return got == len;
}

// Minimal HTTP POST /update handler for signed (optionally encrypted) packages.
//
// Body formats:
//   1) raw signed:  [firmware.bin][64-byte ECDSA sig]
//   2) encrypted:   "ESMOTA1" | flags(1) | reserved(1) | iv(16) | ciphertext | sig(64)
//
// Streaming design: hash (and optionally decrypt) while receiving so large
// images fit in ESP32-C3 RAM. Update.end(true) runs only after signature OK.
static void handleSignedHttpClient(WiFiClient& client) {
  client.setTimeout(30);

  String reqLine = client.readStringUntil('\n');
  reqLine.trim();
  if (reqLine.length() == 0) {
    client.stop();
    return;
  }

  String passwordHeader;
  size_t contentLength = 0;
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      break;
    }
    if (line.startsWith("X-OTA-Password:")) {
      passwordHeader = line.substring(strlen("X-OTA-Password:"));
      passwordHeader.trim();
    } else if (line.startsWith("Content-Length:")) {
      contentLength = (size_t)line.substring(strlen("Content-Length:")).toInt();
    } else if (line.startsWith("Authorization: Bearer ")) {
      passwordHeader = line.substring(strlen("Authorization: Bearer "));
      passwordHeader.trim();
    }
  }

  auto sendResponse = [&](int code, const char* msg) {
    client.printf("HTTP/1.0 %d %s\r\nContent-Type: text/plain\r\nConnection: close\r\nContent-Length: %u\r\n\r\n%s",
                  code, code == 200 ? "OK" : "Error", (unsigned)strlen(msg), msg);
    client.flush();
  };

  if (!reqLine.startsWith("POST /update")) {
    sendResponse(404, "Not Found (use POST /update)");
    client.stop();
    return;
  }

  if (passwordHeader != String(otaPassword)) {
    logOta("Signed HTTP OTA: authentication failed");
    sendResponse(401, "Unauthorized");
    client.stop();
    return;
  }

  if (contentLength < OTA_ECDSA_SIG_LEN + 64) {
    sendResponse(400, "Payload too small");
    client.stop();
    return;
  }

  const size_t MAX_OTA_BYTES = 0x180000;
  if (contentLength > MAX_OTA_BYTES) {
    sendResponse(413, "Payload too large");
    client.stop();
    return;
  }

  const bool mustVerify = otaSignatureRequired() || otaPublicKeyConfigured();
  if (mustVerify && !otaPublicKeyConfigured()) {
    sendResponse(500, "Public key not provisioned");
    client.stop();
    return;
  }

  const size_t signedRegionLen = contentLength - OTA_ECDSA_SIG_LEN;

  // Peek at optional encryption header (first 25 bytes) without large buffers.
  const size_t ENC_HEADER_LEN = OTA_ENC_MAGIC_LEN + 1 + 1 + 16; // 25
  uint8_t headerPeek[ENC_HEADER_LEN];
  if (!readExact(client, headerPeek, ENC_HEADER_LEN)) {
    sendResponse(400, "Incomplete header");
    client.stop();
    return;
  }

  bool encrypted = false;
  uint8_t iv[16];
  size_t payloadOffset = 0; // offset of firmware/ciphertext within signed region
  size_t payloadLen = signedRegionLen;

  if (memcmp(headerPeek, OTA_ENC_MAGIC, OTA_ENC_MAGIC_LEN) == 0) {
    uint8_t flags = headerPeek[OTA_ENC_MAGIC_LEN];
    payloadOffset = ENC_HEADER_LEN;
    if (signedRegionLen <= payloadOffset) {
      sendResponse(400, "Invalid encrypted package");
      client.stop();
      return;
    }
    payloadLen = signedRegionLen - payloadOffset;
    if (flags & OTA_ENC_FLAG_ENCRYPTED) {
      if (!otaAesKeyConfigured()) {
        sendResponse(500, "Encrypted package but AES key not provisioned on device");
        client.stop();
        return;
      }
      memcpy(iv, headerPeek + OTA_ENC_MAGIC_LEN + 2, 16);
      encrypted = true;
    }
  }

  // Begin flash write for the *plaintext* image size.
  if (!Update.begin(payloadLen, U_FLASH)) {
    sendResponse(500, "Update.begin failed");
    client.stop();
    return;
  }

  // Streaming SHA-256 of the full signed region (header + payload).
  mbedtls_sha256_context shaCtx;
  mbedtls_sha256_init(&shaCtx);
#if MBEDTLS_VERSION_MAJOR >= 3
  mbedtls_sha256_starts(&shaCtx, 0);
#else
  mbedtls_sha256_starts_ret(&shaCtx, 0);
#endif

  auto shaUpdate = [&](const uint8_t* data, size_t len) {
#if MBEDTLS_VERSION_MAJOR >= 3
    mbedtls_sha256_update(&shaCtx, data, len);
#else
    mbedtls_sha256_update_ret(&shaCtx, data, len);
#endif
  };

  // Header contributes to signature when using ESMOTA1 packages; for raw
  // signed images the peeked bytes are the start of the firmware itself.
  if (payloadOffset > 0) {
    shaUpdate(headerPeek, ENC_HEADER_LEN);
  }

  // AES-CTR state for optional decrypt-while-write
  mbedtls_aes_context aes;
  uint8_t nonce_counter[16];
  uint8_t stream_block[16];
  size_t nc_off = 0;
  if (encrypted) {
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, OTA_AES_KEY, 256);
    memcpy(nonce_counter, iv, 16);
    memset(stream_block, 0, sizeof(stream_block));
  }

  uint8_t buf[1024];
  size_t remainingSigned = signedRegionLen;
  size_t alreadyConsumed = (payloadOffset > 0) ? ENC_HEADER_LEN : 0;

  // Feed peeked firmware bytes (raw signed path) through hash + flash.
  if (payloadOffset == 0) {
    shaUpdate(headerPeek, ENC_HEADER_LEN);
    size_t w = Update.write(headerPeek, ENC_HEADER_LEN);
    if (w != ENC_HEADER_LEN) {
      Update.abort();
      otaRestoreRunningBootPartition();
      mbedtls_sha256_free(&shaCtx);
      sendResponse(500, "Update.write failed (header)");
      client.stop();
      return;
    }
    alreadyConsumed = ENC_HEADER_LEN;
  }

  remainingSigned = signedRegionLen - alreadyConsumed;

  bool ioError = false;
  while (remainingSigned > 0) {
    size_t chunk = remainingSigned > sizeof(buf) ? sizeof(buf) : remainingSigned;
    if (!readExact(client, buf, chunk)) {
      ioError = true;
      break;
    }
    shaUpdate(buf, chunk);

    // Decrypt in place when encrypted, then write plaintext to flash.
    uint8_t* toWrite = buf;
    if (encrypted) {
      if (mbedtls_aes_crypt_ctr(&aes, chunk, &nc_off, nonce_counter, stream_block, buf, buf) != 0) {
        ioError = true;
        break;
      }
    }

    if (Update.write(toWrite, chunk) != chunk) {
      ioError = true;
      break;
    }
    remainingSigned -= chunk;
  }

  if (encrypted) {
    mbedtls_aes_free(&aes);
  }

  uint8_t signature[OTA_ECDSA_SIG_LEN];
  if (ioError || !readExact(client, signature, OTA_ECDSA_SIG_LEN)) {
    Update.abort();
    otaRestoreRunningBootPartition();
    mbedtls_sha256_free(&shaCtx);
    sendResponse(400, "Incomplete body or write error");
    client.stop();
    return;
  }

  uint8_t hash[32];
#if MBEDTLS_VERSION_MAJOR >= 3
  mbedtls_sha256_finish(&shaCtx, hash);
#else
  mbedtls_sha256_finish_ret(&shaCtx, hash);
#endif
  mbedtls_sha256_free(&shaCtx);

  if (mustVerify) {
    if (!otaVerifyEcdsaP256(hash, signature)) {
      Update.abort();
      otaRestoreRunningBootPartition();
      logOta("Signed HTTP OTA: signature verification FAILED — previous firmware unchanged");
      sendResponse(403, "Signature verification failed");
      client.stop();
      return;
    }
    logOta(encrypted ? "Signed HTTP OTA: signature OK (encrypted package)"
                     : "Signed HTTP OTA: signature OK");
  }

  // Signature verified (or not required) — commit boot partition switch.
  if (!Update.end(true)) {
    otaRestoreRunningBootPartition();
    sendResponse(500, "Update.end failed");
    client.stop();
    return;
  }

  char okMsg[96];
  snprintf(okMsg, sizeof(okMsg), "OK encrypted=%d bytes=%u — rebooting",
           encrypted ? 1 : 0, (unsigned)payloadLen);
  sendResponse(200, okMsg);
  client.stop();

  logOta("Signed HTTP OTA applied successfully — rebooting");
  delay(1500);
  ESP.restart();
}

static void pollSignedHttpOta() {
  if (!signedOtaServerStarted) {
    return;
  }
  WiFiClient client = signedOtaServer.available();
  if (client) {
    logOta("Signed HTTP OTA client connected");
    handleSignedHttpClient(client);
    if (client.connected()) {
      client.stop();
    }
  }
}

void handleOTA() {
  ArduinoOTA.handle();
  pollSignedHttpOta();
}

bool checkRollbackCondition() {
  otaPrefs.begin("ota_rollback", true);
  int bootFailCount = otaPrefs.getInt("boot_fail_count", 0);
  otaPrefs.end();

  return bootFailCount >= 10;
}

void markFirmwareValid() {
  // Mark current partition as valid to prevent rollback
  esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
  if (err == ESP_OK) {
    Serial.printf("[%10lu ms] [OTA] Firmware marked as valid\r\n", millis());
    telnetPrintf("[%10lu ms] [OTA] Firmware marked as valid\r\n", millis());

    // Reset boot failure counter on successful validation
    resetBootFailureCount();
  } else {
    Serial.printf("[%10lu ms] [OTA] Failed to mark firmware as valid: %s\r\n", millis(), esp_err_to_name(err));
    telnetPrintf("[%10lu ms] [OTA] Failed to mark firmware as valid: %s\r\n", millis(), esp_err_to_name(err));
  }
}

void handleOTARollback() {
  if (!checkRollbackCondition()) {
    return;
  }

  // Log rollback event
  Serial.printf("[%10lu ms] [OTA] *** FIRMWARE ROLLBACK TRIGGERED ***\r\n", millis());
  Serial.printf("[%10lu ms] [OTA] Boot failures exceeded threshold (10)\r\n", millis());
  Serial.printf("[%10lu ms] [OTA] Rolling back to previous firmware version\r\n", millis());

  telnetPrintf("[%10lu ms] [OTA] *** FIRMWARE ROLLBACK TRIGGERED ***\r\n", millis());
  telnetPrintf("[%10lu ms] [OTA] Boot failures exceeded threshold (10)\r\n", millis());
  telnetPrintf("[%10lu ms] [OTA] Rolling back to previous firmware version\r\n", millis());

  // Send Pushover alert about rollback
  char alertTitle[100];
  char alertMessage[200];
  snprintf(alertTitle, sizeof(alertTitle), "OTA Rollback - %s", deviceName);
  snprintf(alertMessage, sizeof(alertMessage),
           "Device experienced 10+ boot failures. Rolling back from firmware v%s to previous version. Device will restart.",
           firmwareVersion);

  sendPushoverAlert(alertTitle, alertMessage, 1); // High priority alert

  // Clear boot failure count before rollback
  resetBootFailureCount();

  // Save rollback event to preferences for post-rollback logging
  otaPrefs.begin("ota_rollback", false);
  otaPrefs.putString("last_rollback_from", firmwareVersion);
  otaPrefs.putULong("rollback_time", millis());
  otaPrefs.end();

  delay(2000); // Give time for alert to send

  // Trigger ESP32 OTA rollback
  esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
  if (err != ESP_OK) {
    Serial.printf("[%10lu ms] [OTA] Rollback failed: %s\r\n", millis(), esp_err_to_name(err));
    telnetPrintf("[%10lu ms] [OTA] Rollback failed: %s\r\n", millis(), esp_err_to_name(err));
  }

  // If we reach here, rollback failed - reboot anyway to try recovery
  Serial.printf("[%10lu ms] [OTA] Manual reboot after rollback failure\r\n", millis());
  telnetPrintf("[%10lu ms] [OTA] Manual reboot after rollback failure\r\n", millis());
  delay(1000);
  ESP.restart();
}

int getBootFailureCount() {
  otaPrefs.begin("ota_rollback", true);
  int count = otaPrefs.getInt("boot_fail_count", 0);
  otaPrefs.end();
  return count;
}

void resetBootFailureCount() {
  otaPrefs.begin("ota_rollback", false);
  otaPrefs.putInt("boot_fail_count", 0);
  otaPrefs.end();

  Serial.printf("[%10lu ms] [OTA] Boot failure counter reset\r\n", millis());
}
