#!/usr/bin/env python3
"""Sign (and optionally encrypt) an ESP32 firmware image for secure OTA.

Signed package (espota / ArduinoOTA):
  [firmware.bin][64-byte ECDSA-P256 signature over SHA-256(firmware)]

Encrypted package (HTTP signed OTA on port 8267):
  b"ESMOTA1" + flags(1) + reserved(1) + iv(16) + ciphertext + sig(64)
  Signature covers all bytes before the 64-byte trailer.

Usage:
  python scripts/sign_firmware.py firmware.bin
  python scripts/sign_firmware.py firmware.bin -o firmware.signed.bin
  python scripts/sign_firmware.py firmware.bin --encrypt --aes-key keys/ota_aes.key
  python scripts/sign_firmware.py firmware.bin --private-key keys/ota_private.pem

Upload signed image via PlatformIO/espota:
  pio run -t upload --upload-port poop-monitor.local
  # after pointing upload at the .signed.bin, or:
  python -m espota -i <device-ip> -f firmware.signed.bin -a <ota-password>

Upload encrypted package via HTTP:
  curl -X POST http://poop-monitor.local:8267/update \\
    -H "X-OTA-Password: YOUR_OTA_PASSWORD" \\
    -H "Content-Type: application/octet-stream" \\
    --data-binary @firmware.encrypted.bin
"""

from __future__ import annotations

import argparse
import hashlib
import os
import sys


MAGIC = b"ESMOTA1"
FLAG_ENCRYPTED = 0x01


def _require_cryptography():
    try:
        from cryptography.hazmat.primitives.asymmetric import ec, utils
        from cryptography.hazmat.primitives import hashes, serialization
        from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
        from cryptography.hazmat.backends import default_backend
        return ec, utils, hashes, serialization, Cipher, algorithms, modes, default_backend
    except ImportError:
        print(
            "Missing dependency: cryptography\n"
            "  pip install cryptography",
            file=sys.stderr,
        )
        sys.exit(1)


def _load_private_key(path: str, serialization, default_backend):
    with open(path, "rb") as f:
        data = f.read()
    return serialization.load_pem_private_key(data, password=None, backend=default_backend())


def _load_aes_key(path: str) -> bytes:
    with open(path, "r", encoding="utf-8") as f:
        text = f.read().strip().replace(" ", "").replace("\n", "")
    if len(text) == 64:
        return bytes.fromhex(text)
    raw = open(path, "rb").read()
    if len(raw) == 32:
        return raw
    raise SystemExit(f"AES key file must be 32 raw bytes or 64 hex chars: {path}")


def _ecdsa_sign_sha256(private_key, data: bytes, ec, utils, hashes) -> bytes:
    digest = hashlib.sha256(data).digest()
    sig_der = private_key.sign(digest, ec.ECDSA(utils.Prehashed(hashes.SHA256())))
    r, s = utils.decode_dss_signature(sig_der)
    return r.to_bytes(32, "big") + s.to_bytes(32, "big")


def main() -> int:
    (
        ec,
        utils,
        hashes,
        serialization,
        Cipher,
        algorithms,
        modes,
        default_backend,
    ) = _require_cryptography()

    parser = argparse.ArgumentParser(description="Sign ESP32 firmware for OTA")
    parser.add_argument("firmware", help="Path to firmware.bin")
    parser.add_argument(
        "-o",
        "--output",
        help="Output path (default: <firmware>.signed.bin or .encrypted.bin)",
    )
    parser.add_argument(
        "--private-key",
        default=os.path.join("keys", "ota_private.pem"),
        help="ECDSA P-256 private key PEM (default: keys/ota_private.pem)",
    )
    parser.add_argument(
        "--encrypt",
        action="store_true",
        help="AES-256-CTR encrypt payload (for HTTP OTA on port 8267)",
    )
    parser.add_argument(
        "--aes-key",
        default=os.path.join("keys", "ota_aes.key"),
        help="AES-256 key file (hex or raw 32 bytes)",
    )
    args = parser.parse_args()

    if not os.path.isfile(args.firmware):
        print(f"Firmware not found: {args.firmware}", file=sys.stderr)
        return 1
    if not os.path.isfile(args.private_key):
        print(
            f"Private key not found: {args.private_key}\n"
            "  Run: python scripts/generate_ota_keys.py",
            file=sys.stderr,
        )
        return 1

    with open(args.firmware, "rb") as f:
        image = f.read()

    if len(image) < 256:
        print("Firmware looks too small — aborting", file=sys.stderr)
        return 1

    private_key = _load_private_key(args.private_key, serialization, default_backend)

    if args.encrypt:
        if not os.path.isfile(args.aes_key):
            print(
                f"AES key not found: {args.aes_key}\n"
                "  Run: python scripts/generate_ota_keys.py --aes",
                file=sys.stderr,
            )
            return 1
        aes_key = _load_aes_key(args.aes_key)
        iv = os.urandom(16)
        encryptor = Cipher(
            algorithms.AES(aes_key), modes.CTR(iv), backend=default_backend()
        ).encryptor()
        ciphertext = encryptor.update(image) + encryptor.finalize()
        # magic(7) + flags(1) + reserved(1) + iv(16) + ciphertext
        payload = MAGIC + bytes([FLAG_ENCRYPTED, 0x00]) + iv + ciphertext
        default_out = args.firmware + ".encrypted.bin"
    else:
        payload = image
        default_out = args.firmware + ".signed.bin"

    signature = _ecdsa_sign_sha256(private_key, payload, ec, utils, hashes)
    package = payload + signature

    out_path = args.output or default_out
    with open(out_path, "wb") as f:
        f.write(package)

    print(f"Input:     {args.firmware} ({len(image)} bytes)")
    print(f"Mode:      {'encrypt+sign' if args.encrypt else 'sign-only'}")
    print(f"Output:    {out_path} ({len(package)} bytes)")
    print(f"Sig SHA256 digest of signed region ({len(payload)} bytes):")
    print(f"           {hashlib.sha256(payload).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
