#!/usr/bin/env python3
"""
Test script to validate HTTP-OTA (port 8267) package framing
=============================================================

The device's signed-HTTP handler flashes every uploaded byte except the
last 64 (the signature trailer). Uploading a raw firmware.bin therefore
flashes a truncated image and fails with "Update.end failed". These tests
lock in the framing contract implemented by scripts/package_ota.py:

1. The package is exactly the firmware image plus a 64-byte trailer.
2. The firmware prefix is byte-identical (no truncation possible).
3. Oversize / tiny inputs are rejected before touching the device.
4. The CLI emits a framed package for keyless (password-only) devices.
5. With a real ECDSA key, the trailer verifies over the image.
"""

import hashlib
import os
import subprocess
import sys
import tempfile
import unittest

PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))
PACKAGE_SCRIPT = os.path.join(PROJECT_ROOT, "scripts", "package_ota.py")

sys.path.insert(0, os.path.join(PROJECT_ROOT, "scripts"))
from package_ota import MAX_OTA_BYTES, SIG_LEN, build_package

try:
    from cryptography.hazmat.primitives.asymmetric import ec, utils
    from cryptography.hazmat.primitives import hashes, serialization

    HAS_CRYPTO = True
except ImportError:
    HAS_CRYPTO = False

# Big enough to pass the minimum-image check, small enough to stay fast.
FAKE_FIRMWARE = bytes(range(256)) * 4  # 1024 bytes


class TestOtaPackaging(unittest.TestCase):
    """Framing contract for the 8267 HTTP OTA upload path."""

    def test_trailer_appended_after_image(self):
        trailer = b"T" * SIG_LEN
        package = build_package(FAKE_FIRMWARE, trailer)
        self.assertEqual(len(package), len(FAKE_FIRMWARE) + SIG_LEN)
        self.assertEqual(package[: len(FAKE_FIRMWARE)], FAKE_FIRMWARE)
        self.assertEqual(package[len(FAKE_FIRMWARE) :], trailer)

    def test_trailer_wrong_size_rejected(self):
        with self.assertRaises(ValueError):
            build_package(FAKE_FIRMWARE, b"short")

    def test_tiny_image_rejected(self):
        with self.assertRaises(ValueError):
            build_package(b"tiny", b"Z" * SIG_LEN)

    def test_oversize_package_rejected(self):
        with self.assertRaises(ValueError):
            build_package(b"F" * MAX_OTA_BYTES, b"Z" * SIG_LEN)

    def test_cli_unsigned_package_framing(self):
        with tempfile.TemporaryDirectory() as tmp:
            firmware = os.path.join(tmp, "firmware.bin")
            with open(firmware, "wb") as f:
                f.write(FAKE_FIRMWARE)
            res = subprocess.run(
                [sys.executable, PACKAGE_SCRIPT, firmware],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                cwd=tmp,  # no keys/ here -> unsigned path
            )
            self.assertEqual(res.returncode, 0, msg=res.stderr)
            self.assertIn("unsigned-legacy", res.stdout)
            package_path = firmware + ".ota.bin"
            with open(package_path, "rb") as f:
                package = f.read()
            self.assertEqual(len(package), len(FAKE_FIRMWARE) + SIG_LEN)
            self.assertEqual(package[: len(FAKE_FIRMWARE)], FAKE_FIRMWARE)
            self.assertEqual(package[len(FAKE_FIRMWARE) :], bytes(SIG_LEN))

    def test_cli_require_signed_fails_without_key(self):
        with tempfile.TemporaryDirectory() as tmp:
            firmware = os.path.join(tmp, "firmware.bin")
            with open(firmware, "wb") as f:
                f.write(FAKE_FIRMWARE)
            res = subprocess.run(
                [sys.executable, PACKAGE_SCRIPT, firmware, "--require-signed"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                cwd=tmp,
            )
            self.assertNotEqual(res.returncode, 0)

    @unittest.skipUnless(HAS_CRYPTO, "cryptography package not installed")
    def test_cli_signed_trailer_verifies(self):
        from cryptography.hazmat.backends import default_backend

        key = ec.generate_private_key(ec.SECP256R1(), default_backend())
        with tempfile.TemporaryDirectory() as tmp:
            key_path = os.path.join(tmp, "ota_private.pem")
            with open(key_path, "wb") as f:
                f.write(
                    key.private_bytes(
                        serialization.Encoding.PEM,
                        serialization.PrivateFormat.TraditionalOpenSSL,
                        serialization.NoEncryption(),
                    )
                )
            firmware = os.path.join(tmp, "firmware.bin")
            with open(firmware, "wb") as f:
                f.write(FAKE_FIRMWARE)
            res = subprocess.run(
                [sys.executable, PACKAGE_SCRIPT, firmware, "--private-key", key_path],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                cwd=tmp,
            )
            self.assertEqual(res.returncode, 0, msg=res.stderr)
            self.assertIn("signed", res.stdout)
            with open(firmware + ".signed.bin", "rb") as f:
                package = f.read()
            image, trailer = package[:-SIG_LEN], package[-SIG_LEN:]
            self.assertEqual(image, FAKE_FIRMWARE)
            r = int.from_bytes(trailer[:32], "big")
            s = int.from_bytes(trailer[32:], "big")
            key.public_key().verify(
                utils.encode_dss_signature(r, s),
                hashlib.sha256(image).digest(),
                ec.ECDSA(utils.Prehashed(hashes.SHA256())),
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
