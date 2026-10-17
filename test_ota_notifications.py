#!/usr/bin/env python3
"""
Test script to validate OTA upload notifications and behaviour
==============================================================

Validates that:
1. Pushover alert returns early without network calls when token/user is empty
2. Explicit connect and read timeouts (3000ms) and TLS timeout are set
3. urlEncode properly escapes special characters (&, +, spaces, etc.)
4. Pushover POST payloads correctly encode title, message, and credentials
5. OTA updates are rejected when OTA_PASSWORD is empty or credentials missing
6. Bad password attempts trigger priority 0 alerts and are rate-limited
7. Filesystem OTA is refused when signature enforcement is enabled
"""

import os
import subprocess
import sys
import unittest

PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))
HOST_TEST_SOURCE = os.path.join(PROJECT_ROOT, "tests", "test_ota_host.cpp")
HOST_TEST_BIN = "/tmp/test_ota_host"


class TestOtaBehavioralHost(unittest.TestCase):
    """Behavioral host tests executing compiled C++ firmware logic."""

    @classmethod
    def setUpClass(cls):
        mock_include = os.path.join(PROJECT_ROOT, "tests", "mock_arduino")
        src_include = os.path.join(PROJECT_ROOT, "src")
        compile_cmd = [
            "g++",
            "-std=c++17",
            "-O2",
            f"-I{mock_include}",
            f"-I{src_include}",
            HOST_TEST_SOURCE,
            "-o",
            HOST_TEST_BIN,
        ]
        res = subprocess.run(
            compile_cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if res.returncode != 0:
            err_msg = res.stderr
            raise RuntimeError(
                f"Failed to compile host test harness:\n{err_msg}"
            )

    def run_host_test(self, test_name):
        res = subprocess.run(
            [HOST_TEST_BIN, test_name],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        self.assertEqual(
            res.returncode,
            0,
            f"Host test {test_name} failed:\n{res.stdout}\n{res.stderr}",
        )

    def test_empty_token_returns_early(self):
        """Pushover alert must return early with 0 network calls."""
        self.run_host_test("test_empty_token_returns_early")

    def test_null_token_returns_early(self):
        """Pushover alert must return early when token is null."""
        self.run_host_test("test_null_token_returns_early")

    def test_empty_user_returns_early(self):
        """Pushover alert must return early when user is empty."""
        self.run_host_test("test_empty_user_returns_early")

    def test_null_user_returns_early(self):
        """Pushover alert must return early when user is null."""
        self.run_host_test("test_null_user_returns_early")

    def test_explicit_network_timeouts(self):
        """Pushover alert must configure short explicit timeouts."""
        self.run_host_test("test_explicit_network_timeouts")

    def test_url_encode_function(self):
        """urlEncode must properly encode unreserved chars and symbols."""
        self.run_host_test("test_url_encode_function")

    def test_pushover_post_body_url_encoding(self):
        """Pushover POST body must URL-encode payload fields."""
        self.run_host_test("test_pushover_post_body_url_encoding")

    def test_empty_ota_password_rejected(self):
        """OTA must be rejected when OTA_PASSWORD is empty."""
        self.run_host_test("test_empty_ota_password_rejected")

    def test_auth_failure_rate_limiting_and_priority(self):
        """Bad passwords must send priority 0 alerts and debounce."""
        self.run_host_test("test_auth_failure_rate_limiting_and_priority")

    def test_filesystem_ota_requires_signing_refusal(self):
        """Unsigned filesystem updates must be refused in signing mode."""
        self.run_host_test("test_filesystem_ota_requires_signing_refusal")


def main():
    print("🔍 ESP32 Smart Monitor - OTA Notifications Validation")
    print("=====================================================")
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(
        TestOtaBehavioralHost
    )
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
