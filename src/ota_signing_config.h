#pragma once
#include <stdint.h>

// =============================================================================
// OTA signing / encryption configuration
// See docs/OTA_SIGNING.md for key provisioning and signing workflow.
// =============================================================================

// When 1, OTA images MUST verify against OTA_PUBLIC_KEY_* (reject on failure).
// When 0, verification runs only if a non-zero public key is configured; a missing
// or all-zero key leaves legacy password-only ArduinoOTA behaviour unchanged.
#ifndef OTA_SIGNATURE_ENFORCE
#define OTA_SIGNATURE_ENFORCE 0
#endif

// ECDSA P-256 (secp256r1) public key coordinates (big-endian, 32 bytes each).
// Generate with: python scripts/generate_ota_keys.py
// All zeros = public key not provisioned (signature checks inactive unless ENFORCE).
static const uint8_t OTA_PUBLIC_KEY_X[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t OTA_PUBLIC_KEY_Y[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Optional AES-256 key for encrypted OTA packages (HTTP signed OTA path).
// All zeros = encryption not required / not used.
// Generate with: python scripts/generate_ota_keys.py --aes
static const uint8_t OTA_AES_KEY[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Appended raw signature size (ECDSA P-256 r||s).
#define OTA_ECDSA_SIG_LEN 64

// Magic for optional encrypted package header (HTTP path).
#define OTA_ENC_MAGIC "ESMOTA1"
#define OTA_ENC_MAGIC_LEN 7
#define OTA_ENC_FLAG_ENCRYPTED 0x01
