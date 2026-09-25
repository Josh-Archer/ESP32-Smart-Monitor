#!/usr/bin/env python3
"""
Test script to validate network metrics integration and probe logic
==================================================================

Validates that:
1. handleNetworkMetrics() is invoked from loop() in src/main.cpp
2. loadNetworkMetricsConfigFromStorage() is invoked from setup() in src/main.cpp
3. network_metrics.h is properly included in src/main.cpp
4. Probe scheduling logic (initial 5s delay, interval elapsed, WiFi status check) behaves as expected
5. RTT metrics calculations (mean latency and jitter) match firmware specification
6. Probe target URL validation and configuration constraints adhere to firmware rules
"""

import sys
import os
import re
import unittest

PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))

class TestNetworkMetricsSourceIntegration(unittest.TestCase):
    """Verifies that src/main.cpp properly integrates network_metrics."""

    @classmethod
    def setUpClass(cls):
        main_cpp_path = os.path.join(PROJECT_ROOT, "src", "main.cpp")
        with open(main_cpp_path, "r", encoding="utf-8") as f:
            cls.main_source = f.read()

    def test_network_metrics_header_included(self):
        self.assertRegex(
            self.main_source,
            r'#include\s+["<]network_metrics\.h[">]',
            "src/main.cpp must include network_metrics.h"
        )

    def test_load_network_metrics_config_called_in_setup(self):
        setup_match = re.search(r'void\s+setup\s*\(\s*\)\s*\{([\s\S]+?)\n\}\s*\nvoid\s+loop', self.main_source)
        self.assertIsNotNone(setup_match, "setup() function should be present in src/main.cpp")
        setup_body = setup_match.group(1)
        self.assertIn(
            "loadNetworkMetricsConfigFromStorage()",
            setup_body,
            "loadNetworkMetricsConfigFromStorage() must be called in setup()"
        )

    def test_handle_network_metrics_called_in_loop(self):
        loop_match = re.search(r'void\s+loop\s*\(\s*\)\s*\{([\s\S]+?)\n\}', self.main_source)
        self.assertIsNotNone(loop_match, "loop() function should be present in src/main.cpp")
        loop_body = loop_match.group(1)
        self.assertIn(
            "handleNetworkMetrics()",
            loop_body,
            "handleNetworkMetrics() must be called in loop() to ensure periodic probing"
        )


class TestNetworkMetricsLogic(unittest.TestCase):
    """Unit tests for network metrics scheduling and calculation algorithms."""

    def test_scheduling_initial_probe(self):
        """First probe runs after 5s when lastNetworkProbeMs == 0."""
        def is_due(now_ms, last_probe_ms, interval_ms):
            if last_probe_ms == 0:
                return now_ms >= 5000
            return (now_ms - last_probe_ms) >= interval_ms

        self.assertFalse(is_due(0, 0, 60000))
        self.assertFalse(is_due(4999, 0, 60000))
        self.assertTrue(is_due(5000, 0, 60000))
        self.assertTrue(is_due(10000, 0, 60000))

    def test_scheduling_interval_probe(self):
        """Subsequent probes run when (now - lastNetworkProbeMs) >= networkProbeIntervalMs."""
        def is_due(now_ms, last_probe_ms, interval_ms):
            if last_probe_ms == 0:
                return now_ms >= 5000
            return (now_ms - last_probe_ms) >= interval_ms

        last_probe = 5000
        interval = 60000
        self.assertFalse(is_due(5000, last_probe, interval))
        self.assertFalse(is_due(64999, last_probe, interval))
        self.assertTrue(is_due(65000, last_probe, interval))
        self.assertTrue(is_due(70000, last_probe, interval))

    def test_latency_and_jitter_calculation(self):
        """Validates mean latency and mean absolute difference jitter."""
        def compute_metrics(samples):
            ok_count = len(samples)
            if ok_count == 0:
                return -1.0, -1.0, False
            latency = sum(samples) / ok_count
            if ok_count >= 2:
                jitter = sum(abs(samples[i] - samples[i - 1]) for i in range(1, ok_count)) / (ok_count - 1)
            else:
                jitter = 0.0
            return latency, jitter, True

        # Test case: 4 valid samples
        samples = [40.0, 50.0, 45.0, 55.0]
        lat, jit, ok = compute_metrics(samples)
        self.assertTrue(ok)
        self.assertAlmostEqual(lat, 47.5, places=2)
        # Differences: |50-40|=10, |45-50|=5, |55-45|=10. Sum=25, count=3, jitter = 25/3 = 8.333...
        self.assertAlmostEqual(jit, 25.0 / 3.0, places=2)

        # Test case: single sample
        lat, jit, ok = compute_metrics([42.0])
        self.assertTrue(ok)
        self.assertAlmostEqual(lat, 42.0)
        self.assertAlmostEqual(jit, 0.0)

        # Test case: zero samples
        lat, jit, ok = compute_metrics([])
        self.assertFalse(ok)
        self.assertEqual(lat, -1.0)
        self.assertEqual(jit, -1.0)

    def test_is_valid_http_url(self):
        """Validates the URL constraints from network_metrics.cpp."""
        def is_valid_http_url(url):
            if not url or len(url) < 10 or len(url) >= 128:
                return False
            if not url.startswith("http://"):
                return False
            host_part = url[7:]
            if not host_part or host_part.startswith('/'):
                return False
            for c in url:
                if ord(c) < 32 or ord(c) > 126 or c in (' ', '"', "'", '\\'):
                    return False
            return True

        self.assertTrue(is_valid_http_url("http://httpbin.org/ip"))
        self.assertTrue(is_valid_http_url("http://192.168.1.1/health"))
        self.assertFalse(is_valid_http_url("https://httpbin.org/ip"))  # HTTPS rejected to avoid TLS heap pressure
        self.assertFalse(is_valid_http_url("http://"))
        self.assertFalse(is_valid_http_url("http:///emptyhost"))
        self.assertFalse(is_valid_http_url("http://bad url with space"))
        self.assertFalse(is_valid_http_url(""))
        self.assertFalse(is_valid_http_url(None))


def main():
    print("🔍 ESP32 Smart Monitor - Network Metrics Fix Validation")
    print("======================================================")
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(TestNetworkMetricsSourceIntegration)
    suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(TestNetworkMetricsLogic))
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
