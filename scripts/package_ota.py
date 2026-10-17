#!/usr/bin/env python3
"""Build an HTTP-OTA (port 8267) upload package for poop-monitor.

The device's signed-HTTP handler always frames an upload as
``[firmware][64-byte trailer]`` and flashes everything except the last
64 bytes, so a raw ``firmware.bin`` uploads truncated and fails with
``Update.end failed``. This helper produces a correctly framed package:

* keys present (``keys/ota_private.pem``) -> real ECDSA-P256 signature
  over the image, same format as ``sign_firmware.py`` sign-only mode.
* no keys -> 64 zero bytes as the trailer, for password-only devices
  (``OTA_SIGNATURE_ENFORCE 0`` with no public key provisioned). The
  device skips verification in that mode but still needs the framing.

Usage:
  python scripts/package_ota.py .pio/build/esp32-c3-devkitm-1/firmware.bin
  python scripts/package_ota.py firmware.bin --require-signed
"""

from __future__ import annotations

import argparse
import os
import sys

SIG_LEN = 64
# Mirrors MAX_OTA_BYTES / the minimum-size check in src/ota_manager.cpp.
MAX_OTA_BYTES = 0x180000
MIN_IMAGE_BYTES = 128


def build_package(image: bytes, trailer: bytes) -> bytes:
    """Return ``image + trailer`` for the 8267 upload path."""
    if len(trailer) != SIG_LEN:
        raise ValueError(f"trailer must be {SIG_LEN} bytes, got {len(trailer)}")
    if len(image) < MIN_IMAGE_BYTES:
        raise ValueError(f"image too small ({len(image)} bytes)")
    package = image + trailer
    if len(package) > MAX_OTA_BYTES:
        raise ValueError(f"package too large ({len(package)} bytes)")
    return package


def sign_image(image: bytes, private_key_path: str) -> bytes:
    """ECDSA-P256 (r||s) signature over SHA-256(image)."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from sign_firmware import _ecdsa_sign_sha256, _load_private_key, _require_cryptography

    (ec, utils, hashes, serialization, _, _, _, default_backend) = _require_cryptography()
    key = _load_private_key(private_key_path, serialization, default_backend)
    return _ecdsa_sign_sha256(key, image, ec, utils, hashes)


def main() -> int:
    parser = argparse.ArgumentParser(description="Frame firmware for HTTP OTA upload")
    parser.add_argument("firmware", help="Path to firmware.bin")
    parser.add_argument("-o", "--output", help="Output path (default derived)")
    parser.add_argument(
        "--private-key",
        default=os.path.join("keys", "ota_private.pem"),
        help="ECDSA P-256 private key PEM (default: keys/ota_private.pem)",
    )
    parser.add_argument(
        "--require-signed",
        action="store_true",
        help="Fail instead of emitting an unsigned (zero-trailer) package",
    )
    args = parser.parse_args()

    if not os.path.isfile(args.firmware):
        print(f"Firmware not found: {args.firmware}", file=sys.stderr)
        return 1
    with open(args.firmware, "rb") as f:
        image = f.read()

    if os.path.isfile(args.private_key):
        trailer = sign_image(image, args.private_key)
        mode = "signed"
        default_out = args.firmware + ".signed.bin"
    elif args.require_signed:
        print(
            f"Private key not found: {args.private_key}\n"
            "  Run: python scripts/generate_ota_keys.py",
            file=sys.stderr,
        )
        return 1
    else:
        print(
            "WARNING: no private key found — emitting UNSIGNED package "
            "(zero trailer) for password-only devices. "
            "Signed devices will reject it.",
            file=sys.stderr,
        )
        trailer = bytes(SIG_LEN)
        mode = "unsigned-legacy"
        default_out = args.firmware + ".ota.bin"

    try:
        package = build_package(image, trailer)
    except ValueError as exc:
        print(f"Cannot package firmware: {exc}", file=sys.stderr)
        return 1

    out_path = args.output or default_out
    with open(out_path, "wb") as f:
        f.write(package)
    print(f"Input:  {args.firmware} ({len(image)} bytes)")
    print(f"Mode:   {mode}")
    print(f"Output: {out_path} ({len(package)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
