#include "ota_crypto.h"
#include "ota_signing_config.h"
#include "telnet.h"

#include <esp_ota_ops.h>
#include <string.h>

#include "mbedtls/version.h"
#include "mbedtls/sha256.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecp.h"
#include "mbedtls/bignum.h"
#include "mbedtls/aes.h"

static bool isAllZero(const uint8_t* buf, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (buf[i] != 0) {
      return false;
    }
  }
  return true;
}

bool otaPublicKeyConfigured() {
  return !isAllZero(OTA_PUBLIC_KEY_X, sizeof(OTA_PUBLIC_KEY_X)) &&
         !isAllZero(OTA_PUBLIC_KEY_Y, sizeof(OTA_PUBLIC_KEY_Y));
}

bool otaAesKeyConfigured() {
  return !isAllZero(OTA_AES_KEY, sizeof(OTA_AES_KEY));
}

bool otaSignatureRequired() {
#if OTA_SIGNATURE_ENFORCE
  return true;
#else
  return otaPublicKeyConfigured();
#endif
}

const char* otaSigningStatusString() {
  if (otaSignatureRequired()) {
    if (!otaPublicKeyConfigured()) {
      return "enforced-but-no-key";
    }
    return otaAesKeyConfigured() ? "signed+encrypt-ready" : "signed-required";
  }
  if (otaPublicKeyConfigured()) {
    return otaAesKeyConfigured() ? "signed+encrypt-optional" : "signed-optional";
  }
  return "password-only";
}

bool otaSha256Buffer(const uint8_t* data, size_t length, uint8_t outHash[32]) {
  if (!data || !outHash) {
    return false;
  }
#if MBEDTLS_VERSION_MAJOR >= 3
  int ret = mbedtls_sha256(data, length, outHash, 0);
#else
  int ret = mbedtls_sha256_ret(data, length, outHash, 0);
#endif
  return ret == 0;
}

bool otaSha256Partition(const esp_partition_t* part, size_t offset, size_t length,
                        uint8_t outHash[32]) {
  if (!part || !outHash) {
    return false;
  }
  if (offset + length > part->size) {
    return false;
  }

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
#if MBEDTLS_VERSION_MAJOR >= 3
  if (mbedtls_sha256_starts(&ctx, 0) != 0) {
#else
  if (mbedtls_sha256_starts_ret(&ctx, 0) != 0) {
#endif
    mbedtls_sha256_free(&ctx);
    return false;
  }

  uint8_t buf[1024];
  size_t remaining = length;
  size_t pos = offset;
  while (remaining > 0) {
    size_t chunk = remaining > sizeof(buf) ? sizeof(buf) : remaining;
    if (esp_partition_read(part, pos, buf, chunk) != ESP_OK) {
      mbedtls_sha256_free(&ctx);
      return false;
    }
#if MBEDTLS_VERSION_MAJOR >= 3
    if (mbedtls_sha256_update(&ctx, buf, chunk) != 0) {
#else
    if (mbedtls_sha256_update_ret(&ctx, buf, chunk) != 0) {
#endif
      mbedtls_sha256_free(&ctx);
      return false;
    }
    pos += chunk;
    remaining -= chunk;
  }

#if MBEDTLS_VERSION_MAJOR >= 3
  int fin = mbedtls_sha256_finish(&ctx, outHash);
#else
  int fin = mbedtls_sha256_finish_ret(&ctx, outHash);
#endif
  mbedtls_sha256_free(&ctx);
  return fin == 0;
}

bool otaVerifyEcdsaP256(const uint8_t hash[32], const uint8_t signature[OTA_ECDSA_SIG_LEN]) {
  if (!hash || !signature || !otaPublicKeyConfigured()) {
    return false;
  }

  // Use group + point APIs (portable across mbedTLS 2.x and 3.x).
  mbedtls_ecp_group grp;
  mbedtls_ecp_point Q;
  mbedtls_mpi r, s;
  mbedtls_ecp_group_init(&grp);
  mbedtls_ecp_point_init(&Q);
  mbedtls_mpi_init(&r);
  mbedtls_mpi_init(&s);

  bool ok = false;
  do {
    if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) != 0) {
      break;
    }

    // Uncompressed SEC1 point: 0x04 || X || Y
    uint8_t pub[65];
    pub[0] = 0x04;
    memcpy(pub + 1, OTA_PUBLIC_KEY_X, 32);
    memcpy(pub + 33, OTA_PUBLIC_KEY_Y, 32);
    if (mbedtls_ecp_point_read_binary(&grp, &Q, pub, sizeof(pub)) != 0) {
      break;
    }
    if (mbedtls_ecp_check_pubkey(&grp, &Q) != 0) {
      break;
    }
    if (mbedtls_mpi_read_binary(&r, signature, 32) != 0) {
      break;
    }
    if (mbedtls_mpi_read_binary(&s, signature + 32, 32) != 0) {
      break;
    }

    int ret = mbedtls_ecdsa_verify(&grp, hash, 32, &Q, &r, &s);
    ok = (ret == 0);
  } while (false);

  mbedtls_mpi_free(&r);
  mbedtls_mpi_free(&s);
  mbedtls_ecp_point_free(&Q);
  mbedtls_ecp_group_free(&grp);
  return ok;
}

bool otaVerifyPartitionSignature(const esp_partition_t* part, size_t totalWritten,
                                 size_t* imageSizeOut) {
  if (!part || totalWritten <= OTA_ECDSA_SIG_LEN) {
    return false;
  }
  if (!otaPublicKeyConfigured()) {
    return false;
  }
  if (totalWritten > part->size) {
    return false;
  }

  const size_t imageSize = totalWritten - OTA_ECDSA_SIG_LEN;
  uint8_t hash[32];
  if (!otaSha256Partition(part, 0, imageSize, hash)) {
    Serial.printf("[%10lu ms] [OTA] Signature verify: failed to hash partition\r\n", millis());
    return false;
  }

  uint8_t signature[OTA_ECDSA_SIG_LEN];
  if (esp_partition_read(part, imageSize, signature, OTA_ECDSA_SIG_LEN) != ESP_OK) {
    Serial.printf("[%10lu ms] [OTA] Signature verify: failed to read signature trailer\r\n", millis());
    return false;
  }

  if (!otaVerifyEcdsaP256(hash, signature)) {
    Serial.printf("[%10lu ms] [OTA] Signature verify: ECDSA check FAILED\r\n", millis());
    telnetPrintf("[%10lu ms] [OTA] Signature verify: ECDSA check FAILED\r\n", millis());
    return false;
  }

  if (imageSizeOut) {
    *imageSizeOut = imageSize;
  }

  Serial.printf("[%10lu ms] [OTA] Signature verify: OK (%u-byte image)\r\n",
                millis(), (unsigned)imageSize);
  telnetPrintf("[%10lu ms] [OTA] Signature verify: OK (%u-byte image)\r\n",
               millis(), (unsigned)imageSize);
  return true;
}

bool otaAesCtrCrypt(const uint8_t iv[16], uint8_t* data, size_t length) {
  if (!iv || !data || !otaAesKeyConfigured()) {
    return false;
  }

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  int ret = mbedtls_aes_setkey_enc(&aes, OTA_AES_KEY, 256);
  if (ret != 0) {
    mbedtls_aes_free(&aes);
    return false;
  }

  uint8_t stream_block[16];
  uint8_t nonce_counter[16];
  memcpy(nonce_counter, iv, 16);
  size_t nc_off = 0;

  ret = mbedtls_aes_crypt_ctr(&aes, length, &nc_off, nonce_counter, stream_block, data, data);
  mbedtls_aes_free(&aes);
  return ret == 0;
}

bool otaRestoreRunningBootPartition() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (!running) {
    Serial.printf("[%10lu ms] [OTA] Cannot restore boot partition: no running partition\r\n", millis());
    return false;
  }

  esp_err_t err = esp_ota_set_boot_partition(running);
  if (err != ESP_OK) {
    Serial.printf("[%10lu ms] [OTA] Failed to restore boot partition: %s\r\n",
                  millis(), esp_err_to_name(err));
    telnetPrintf("[%10lu ms] [OTA] Failed to restore boot partition: %s\r\n",
                 millis(), esp_err_to_name(err));
    return false;
  }

  Serial.printf("[%10lu ms] [OTA] Boot partition restored to running app (previous firmware stays bootable)\r\n",
                millis());
  telnetPrintf("[%10lu ms] [OTA] Boot partition restored — previous firmware remains bootable\r\n",
               millis());
  return true;
}
