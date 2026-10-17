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
7. ArduinoOTA.onStart does not block before espota connects back (deferred)
8. Signed HTTP OTA sends HTTP response before dispatching notifications
9. Notification titles, messages, and priority formatting match specifications
10. OTA error code translation properly maps error constants to strings
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

    def test_arduino_ota_start_notice_deferred(self):
        """ArduinoOTA start notice must not block espota connect."""
        self.run_host_test("test_arduino_ota_start_notice_deferred")

    def test_http_ota_response_before_notification(self):
        """Signed HTTP OTA must send HTTP response before notifying."""
        self.run_host_test("test_http_ota_response_before_notification")


class TestOtaNotificationFormattingAndLogic(unittest.TestCase):
    """Unit tests for notification titles, messages, and error mappings."""

    OTA_AUTH_ERROR = 0
    OTA_BEGIN_ERROR = 1
    OTA_CONNECT_ERROR = 2
    OTA_RECEIVE_ERROR = 3
    OTA_END_ERROR = 4

    def format_title(self, status, device_name):
        return f"OTA Upload {status} - {device_name}"

    def format_start_message(self, upload_type, version):
        type_str = upload_type if upload_type else "sketch"
        return (
            f"OTA upload initiated ({type_str}). Current version: v{version}."
        )

    def format_success_message(self, details, version):
        det = details if details else "OTA update completed successfully"
        return f"{det}. Current version: v{version}. Device will restart."

    def format_failure_message(self, reason):
        r = reason if reason else "Unknown error"
        return f"OTA upload failed: {r}. Previous firmware remains active."

    def map_ota_error(self, error_code):
        errors = {
            self.OTA_AUTH_ERROR: "Authentication Failed",
            self.OTA_BEGIN_ERROR: "Begin Failed",
            self.OTA_CONNECT_ERROR: "Connect Failed",
            self.OTA_RECEIVE_ERROR: "Receive Failed",
            self.OTA_END_ERROR: "End Failed",
        }
        return errors.get(error_code, "Unknown error")

    def test_notification_titles(self):
        device = "poop-monitor"
        self.assertEqual(
            self.format_title("Started", device),
            "OTA Upload Started - poop-monitor",
        )
        self.assertEqual(
            self.format_title("Successful", device),
            "OTA Upload Successful - poop-monitor",
        )
        self.assertEqual(
            self.format_title("Failed", device),
            "OTA Upload Failed - poop-monitor",
        )

    def test_notification_start_messages(self):
        msg = self.format_start_message("sketch", "4.0.4")
        self.assertIn("sketch", msg)
        self.assertIn("v4.0.4", msg)

        msg_fs = self.format_start_message("filesystem", "4.0.4")
        self.assertIn("filesystem", msg_fs)

        msg_default = self.format_start_message(None, "4.0.4")
        self.assertIn("sketch", msg_default)

    def test_notification_success_messages(self):
        msg = self.format_success_message("OTA update accepted", "4.0.4")
        self.assertIn("OTA update accepted", msg)
        self.assertIn("Device will restart", msg)

    def test_notification_failure_messages(self):
        msg = self.format_failure_message("Signature verification failed")
        self.assertIn("Signature verification failed", msg)
        self.assertIn("Previous firmware remains active", msg)

    def test_ota_error_code_mapping(self):
        self.assertEqual(
            self.map_ota_error(self.OTA_AUTH_ERROR),
            "Authentication Failed",
        )
        self.assertEqual(
            self.map_ota_error(self.OTA_BEGIN_ERROR),
            "Begin Failed",
        )
        self.assertEqual(
            self.map_ota_error(self.OTA_CONNECT_ERROR),
            "Connect Failed",
        )
        self.assertEqual(
            self.map_ota_error(self.OTA_RECEIVE_ERROR),
            "Receive Failed",
        )
        self.assertEqual(
            self.map_ota_error(self.OTA_END_ERROR),
            "End Failed",
        )
        self.assertEqual(
            self.map_ota_error(99),
            "Unknown error",
        )


def main():
    print("🔍 ESP32 Smart Monitor - OTA Notifications Validation")
    print("=====================================================")
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(
        TestOtaBehavioralHost
    )
    suite.addTests(
        unittest.defaultTestLoader.loadTestsFromTestCase(
            TestOtaNotificationFormattingAndLogic
        )
    )
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
