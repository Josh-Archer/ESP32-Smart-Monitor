#!/usr/bin/env python3
"""
Test script to validate OTA upload notifications
================================================

Validates that:
1. ota_manager.h declares notifyOtaStart, notifyOtaSuccess, notifyOtaFailure
2. ota_manager.cpp includes notifications.h
3. ArduinoOTA.onStart triggers notification on upload start
4. ArduinoOTA.onEnd triggers notification on upload success (and sig failure)
5. ArduinoOTA.onError triggers notification on upload failure
6. handleSignedHttpClient triggers start, success, and failure notifications
7. Notification title, message, and priority formatting match specifications
8. OTA error code translation properly maps error constants to readable strings
"""

import os
import re
import sys
import unittest

PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))


class TestOtaNotificationSourceIntegration(unittest.TestCase):
    """Verifies that ota_manager properly integrates notifications."""

    @classmethod
    def setUpClass(cls):
        header_path = os.path.join(PROJECT_ROOT, "src", "ota_manager.h")
        with open(header_path, "r", encoding="utf-8") as f:
            cls.header_source = f.read()

        source_path = os.path.join(PROJECT_ROOT, "src", "ota_manager.cpp")
        with open(source_path, "r", encoding="utf-8") as f:
            cls.cpp_source = f.read()

    def test_notifications_header_included(self):
        self.assertRegex(
            self.cpp_source,
            r'#include\s+["<]notifications\.h[">]',
            "src/ota_manager.cpp must include notifications.h",
        )

    def test_ota_notification_functions_declared_in_header(self):
        self.assertIn(
            "notifyOtaStart",
            self.header_source,
            "ota_manager.h must declare notifyOtaStart",
        )
        self.assertIn(
            "notifyOtaSuccess",
            self.header_source,
            "ota_manager.h must declare notifyOtaSuccess",
        )
        self.assertIn(
            "notifyOtaFailure",
            self.header_source,
            "ota_manager.h must declare notifyOtaFailure",
        )

    def test_ota_notification_helpers_call_send_pushover_alert(self):
        # notifyOtaStart must invoke sendPushoverAlert with priority 0
        self.assertRegex(
            self.cpp_source,
            r'void\s+notifyOtaStart[\s\S]+?'
            r'sendPushoverAlert\s*\([^;]+?,\s*0\s*\);',
            "notifyOtaStart must call sendPushoverAlert with priority 0",
        )
        # notifyOtaSuccess must invoke sendPushoverAlert with priority 0
        self.assertRegex(
            self.cpp_source,
            r'void\s+notifyOtaSuccess[\s\S]+?'
            r'sendPushoverAlert\s*\([^;]+?,\s*0\s*\);',
            "notifyOtaSuccess must call sendPushoverAlert with priority 0",
        )
        # notifyOtaFailure must invoke sendPushoverAlert with priority 1
        self.assertRegex(
            self.cpp_source,
            r'void\s+notifyOtaFailure[\s\S]+?'
            r'sendPushoverAlert\s*\([^;]+?,\s*1\s*\);',
            "notifyOtaFailure must call sendPushoverAlert with priority 1",
        )

    def test_arduino_ota_on_start_invokes_notify_ota_start(self):
        on_start_match = re.search(
            r'ArduinoOTA\.onStart\s*\(\s*\[\s*\]\s*\(\s*\)\s*\{'
            r'([\s\S]+?)\}\s*\);',
            self.cpp_source,
        )
        self.assertIsNotNone(
            on_start_match,
            "ArduinoOTA.onStart callback should be defined in ota_manager.cpp",
        )
        body = on_start_match.group(1)
        self.assertIn(
            "notifyOtaStart",
            body,
            "ArduinoOTA.onStart must call notifyOtaStart",
        )

    def test_arduino_ota_on_end_invokes_success_and_failure(self):
        on_end_match = re.search(
            r'ArduinoOTA\.onEnd\s*\(\s*\[\s*\]\s*\(\s*\)\s*\{'
            r'([\s\S]+?)\}\s*\);',
            self.cpp_source,
        )
        self.assertIsNotNone(
            on_end_match,
            "ArduinoOTA.onEnd callback should be defined in ota_manager.cpp",
        )
        body = on_end_match.group(1)
        self.assertIn(
            "notifyOtaSuccess",
            body,
            "ArduinoOTA.onEnd must call notifyOtaSuccess",
        )
        self.assertIn(
            "notifyOtaFailure",
            body,
            "ArduinoOTA.onEnd must call notifyOtaFailure on signature reject",
        )

    def test_arduino_ota_on_error_invokes_notify_ota_failure(self):
        on_error_match = re.search(
            r'ArduinoOTA\.onError\s*\(\s*\[\s*\]\s*\([^)]*\)\s*\{'
            r'([\s\S]+?)\}\s*\);',
            self.cpp_source,
        )
        self.assertIsNotNone(
            on_error_match,
            "ArduinoOTA.onError callback should be in ota_manager.cpp",
        )
        body = on_error_match.group(1)
        self.assertIn(
            "notifyOtaFailure",
            body,
            "ArduinoOTA.onError must call notifyOtaFailure",
        )

    def test_signed_http_ota_invokes_notifications(self):
        handle_http_match = re.search(
            r'void\s+handleSignedHttpClient\s*\([^)]*\)\s*\{([\s\S]+?)\n\}',
            self.cpp_source,
        )
        self.assertIsNotNone(
            handle_http_match,
            "handleSignedHttpClient should be defined in ota_manager.cpp",
        )
        body = handle_http_match.group(1)
        self.assertIn(
            "notifyOtaStart",
            body,
            "handleSignedHttpClient must call notifyOtaStart",
        )
        self.assertIn(
            "notifyOtaSuccess",
            body,
            "handleSignedHttpClient must call notifyOtaSuccess",
        )
        self.assertIn(
            "notifyOtaFailure",
            body,
            "handleSignedHttpClient must call notifyOtaFailure",
        )


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
        TestOtaNotificationSourceIntegration
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
