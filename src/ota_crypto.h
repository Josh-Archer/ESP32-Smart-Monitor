#ifndef OTA_CRYPTO_H
#define OTA_CRYPTO_H

#include <Arduino.h>
#include <esp_partition.h>
#include "ota_signing_config.h"

// Returns true if a non-zero ECDSA public key is embedded in firmware.
bool otaPublicKeyConfigured();

// Returns true if a non-zero AES-256 key is embedded (encrypted packages expected).
bool otaAesKeyConfigured();

// Returns true when signature verification must succeed for OTA to apply.
bool otaSignatureRequired();

// SHA-256 over [offset, offset+length) of a partition.
bool otaSha256Partition(const esp_partition_t* part, size_t offset, size_t length,
                        uint8_t outHash[32]);

// SHA-256 over a RAM buffer.
bool otaSha256Buffer(const uint8_t* data, size_t length, uint8_t outHash[32]);

// Verify ECDSA P-256 signature (raw r||s, 64 bytes) over a SHA-256 digest
// using the embedded public key.
bool otaVerifyEcdsaP256(const uint8_t hash[32], const uint8_t signature[OTA_ECDSA_SIG_LEN]);

// Verify that a freshly written OTA partition matches an appended signature.
// totalWritten = full stream size including the 64-byte signature trailer.
// On success, *imageSizeOut (if non-null) receives the firmware image size
// (totalWritten - OTA_ECDSA_SIG_LEN).
bool otaVerifyPartitionSignature(const esp_partition_t* part, size_t totalWritten,
                                 size_t* imageSizeOut = nullptr);

// AES-256-CTR decrypt in place (IV is 16 bytes).
bool otaAesCtrCrypt(const uint8_t iv[16], uint8_t* data, size_t length);

// Restore boot partition to the currently running app (used after failed verify).
// Leaves the previous firmware bootable; does not reboot.
bool otaRestoreRunningBootPartition();

// Human-readable status for logs / status API.
const char* otaSigningStatusString();

#endif
