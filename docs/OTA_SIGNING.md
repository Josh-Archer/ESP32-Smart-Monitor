# Signed / Encrypted OTA Updates

This project supports **ECDSA P-256 signature verification** before an OTA image is applied, with **optional AES-256-CTR encryption** for the HTTP update path. Failed verification **does not reboot into the new image** and restores the previous boot partition so the device stays on known-good firmware. Existing boot-failure rollback (`esp_ota_mark_app_*`) continues to work after a signed update is accepted.

## Goals (issue #29)

| Requirement | Behaviour |
|---|---|
| Signature verification before apply | New sketch images are hashed and checked with the embedded public key before reboot |
| Document key provisioning | This document + `scripts/generate_ota_keys.py` |
| Failure leaves previous firmware bootable | Boot partition is restored to the running app; no reboot into bad image |
| Works with existing rollback support | Successful boots still call `markFirmwareValid()`; 10 failed boots still roll back |

## Architecture

### Path A — ArduinoOTA / espota (PlatformIO default)

1. Client authenticates with the existing OTA password.
2. Image is written to the inactive OTA partition (including a **64-byte signature trailer**).
3. On download complete, firmware computes `SHA-256` over the image (all bytes except the last 64) and verifies an **ECDSA P-256** signature with the embedded public key.
4. **Pass** → reboot into new firmware (still subject to rollback protection).
5. **Fail** → restore boot partition to the currently running app, stay up, log rejection.

### Path B — Signed HTTP OTA (port **8267**)

Always available (MQTT-only and web builds):

```http
POST /update HTTP/1.0
X-OTA-Password: <OTA_PASSWORD>
Content-Length: <n>
Content-Type: application/octet-stream

<package bytes>
```

Verification runs **before** `Update.begin` / flash write for this path. Optional encryption is supported only here.

## Package formats

### Signed only (espota-friendly)

```
[ ESP32 firmware.bin ][ 64-byte ECDSA signature r||s ]
```

Signature = ECDSA-P256 over `SHA-256(firmware.bin)`, raw 32-byte `r` || 32-byte `s` (not DER).

### Encrypted + signed (HTTP port 8267)

```
"ESMOTA1" | flags(1) | reserved(1) | iv(16) | ciphertext | sig(64)
```

- `flags` bit0 (`0x01`) = AES-256-CTR encrypted payload  
- Signature covers everything **before** the 64-byte trailer  
- Device verifies signature, then decrypts with the embedded AES key, then flashes plaintext

## Key provisioning

### 1. Generate keys (once per product / fleet)

```bash
pip install cryptography
python scripts/generate_ota_keys.py --aes
```

Creates (default directory `keys/`):

| File | Purpose |
|---|---|
| `ota_private.pem` | Signing private key — **never commit** |
| `ota_public.pem` | Public key (PEM) |
| `ota_public.h` | C arrays for embedding |
| `ota_aes.key` | Optional AES-256 key (hex) — **never commit** |

`keys/` is gitignored.

### 2. Embed the public key in firmware

Edit `src/ota_signing_config.h`:

1. Replace `OTA_PUBLIC_KEY_X` / `OTA_PUBLIC_KEY_Y` with the arrays from `keys/ota_public.h`.
2. Optionally copy `OTA_AES_KEY` for encrypted packages.
3. Set `#define OTA_SIGNATURE_ENFORCE 1` when you want OTA to **require** a configured key (fail closed).

### 3. First flash with the new public key

Because the **running** firmware verifies the **next** image, install the public-key-bearing build over **serial** (or a trusted path) once:

```bash
pio run -e esp32-c3-devkitm-1-serial -t upload --upload-port COMx
```

After that, only signed packages are accepted (when a key is configured / enforce is on).

### 4. Sign each release

```bash
pio run -e esp32-c3-devkitm-1
python scripts/sign_firmware.py .pio/build/esp32-c3-devkitm-1/firmware.bin
# → firmware.bin.signed.bin
```

Encrypted (HTTP path):

```bash
python scripts/sign_firmware.py .pio/build/esp32-c3-devkitm-1/firmware.bin --encrypt
# → firmware.bin.encrypted.bin
```

### 5. Upload

**espota (signed only):**

```bash
python -m espota -i poop-monitor.local -p 3232 -a "YOUR_OTA_PASSWORD" \
  -f .pio/build/esp32-c3-devkitm-1/firmware.bin.signed.bin
```

**HTTP signed / encrypted:**

```bash
curl -X POST "http://poop-monitor.local:8267/update" \
  -H "X-OTA-Password: YOUR_OTA_PASSWORD" \
  -H "Content-Type: application/octet-stream" \
  --data-binary @.pio/build/esp32-c3-devkitm-1/firmware.bin.encrypted.bin
```

## Behaviour matrix

| Public key in firmware | `OTA_SIGNATURE_ENFORCE` | Unsigned image | Signed image |
|---|---|---|---|
| All zeros | 0 | Accepted (legacy password-only) | N/A |
| All zeros | 1 | **Rejected** | **Rejected** (no key) |
| Configured | 0 or 1 | **Rejected** | Accepted if signature valid |

## Failure & rollback

1. **Bad signature / auth failure**  
   - Boot partition remains (or is restored to) the running app.  
   - Device does **not** reboot into the rejected image.  
   - Previous firmware stays bootable immediately.

2. **Valid signature but bad firmware (crash loop)**  
   - Existing logic increments boot failure count.  
   - After 10 consecutive failures, `handleOTARollback()` calls  
     `esp_ota_mark_app_invalid_rollback_and_reboot()`.  
   - Signed OTA does not disable this path; `markFirmwareValid()` still runs after a healthy boot.

## Status logging

At boot / OTA start the device logs a signing mode string:

| String | Meaning |
|---|---|
| `password-only` | No public key; legacy OTA |
| `signed-optional` / `signed-required` | ECDSA configured |
| `signed+encrypt-ready` | ECDSA + AES key present |
| `enforced-but-no-key` | Misconfiguration — OTA will reject |

## Security notes

- OTA **password** is still required (network gate). Signature proves **who built** the image.
- Private keys and AES keys must stay offline / in a secrets store (Bitwarden, CI secret, etc.).
- Rotating keys requires a transitional firmware that trusts both old and new public keys, or a serial re-flash.
- Encryption protects firmware **confidentiality in transit/at rest on the update server**; it is optional.
- This is **application-level** signing. Full **secure boot / flash encryption** (eFuse) is a separate hardware hardening step and is not required for these checks.

## Files

| Path | Role |
|---|---|
| `src/ota_signing_config.h` | Embedded public key + AES key + enforce flag |
| `src/ota_crypto.*` | SHA-256, ECDSA verify, AES-CTR, boot restore |
| `src/ota_manager.*` | ArduinoOTA hooks + HTTP signed OTA server |
| `scripts/generate_ota_keys.py` | Key generation |
| `scripts/sign_firmware.py` | Sign / encrypt packages |
