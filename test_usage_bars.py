#!/usr/bin/env python3
"""
Test script to validate Heap, Flash, and RSSI usage bars
=========================================================

Validates that:
1. features.md documents completed status for usage bars referencing issue #40
2. src/web_server.cpp exposes total_heap, flash_size, and flash_used in /status
3. web/index.html includes elements for RSSI, Heap Memory, and Flash Storage bars
4. web/app.js defines methods: updateRssiBar, updateHeapBar, updateFlashBar, drawUsageBars, formatBytes
5. Execution via Node.js validates DOM updates, bar widths, colors, and fallback logic
"""

import os
import re
import sys
import json
import unittest
import subprocess

PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))


class TestUsageBarsSourceIntegration(unittest.TestCase):
    """Verifies that features.md, web_server.cpp, index.html, and app.js are integrated properly."""

    def test_features_md_documents_usage_bars_done(self):
        features_path = os.path.join(PROJECT_ROOT, "features.md")
        with open(features_path, "r", encoding="utf-8") as f:
            content = f.read()
        self.assertRegex(
            content,
            r'Usage bars for memory/cpu/flash.*(?:Done|#40)',
            "features.md must document completion of usage bars referencing issue #40",
        )

    def test_web_server_cpp_exposes_heap_and_flash_metrics(self):
        web_server_cpp = os.path.join(PROJECT_ROOT, "src", "web_server.cpp")
        with open(web_server_cpp, "r", encoding="utf-8") as f:
            content = f.read()

        status_func_match = re.search(
            r'void\s+handleStatus\s*\(\s*\)\s*\{([\s\S]+?)\n\}', content
        )
        self.assertIsNotNone(
            status_func_match, "handleStatus function should be in src/web_server.cpp"
        )
        status_body = status_func_match.group(1)

        self.assertIn('doc["free_heap"]', status_body, "handleStatus must include free_heap")
        self.assertIn('doc["total_heap"]', status_body, "handleStatus must include total_heap")
        self.assertIn('doc["flash_size"]', status_body, "handleStatus must include flash_size")
        self.assertIn('doc["flash_used"]', status_body, "handleStatus must include flash_used")
        self.assertIn('doc["wifi_rssi"]', status_body, "handleStatus must include wifi_rssi")

    def test_index_html_contains_usage_bars_elements(self):
        index_html = os.path.join(PROJECT_ROOT, "web", "index.html")
        with open(index_html, "r", encoding="utf-8") as f:
            content = f.read()

        self.assertIn('id="rssi-bar"', content, "web/index.html must contain #rssi-bar")
        self.assertIn('id="heap-bar"', content, "web/index.html must contain #heap-bar")
        self.assertIn('id="flash-bar"', content, "web/index.html must contain #flash-bar")
        self.assertIn('id="heap-usage"', content, "web/index.html must contain #heap-usage")
        self.assertIn('id="flash-usage"', content, "web/index.html must contain #flash-usage")
        self.assertIn('id="system-resources-row"', content, "web/index.html must contain #system-resources-row")

    def test_app_js_defines_bar_methods(self):
        app_js = os.path.join(PROJECT_ROOT, "web", "app.js")
        with open(app_js, "r", encoding="utf-8") as f:
            content = f.read()

        self.assertIn('updateRssiBar()', content, "web/app.js must define updateRssiBar()")
        self.assertIn('updateHeapBar()', content, "web/app.js must define updateHeapBar()")
        self.assertIn('updateFlashBar()', content, "web/app.js must define updateFlashBar()")
        self.assertIn('drawUsageBars()', content, "web/app.js must define drawUsageBars()")
        self.assertIn('formatBytes(', content, "web/app.js must define formatBytes()")
        self.assertIn('createRssiBarElement', content, "web/app.js must define createRssiBarElement")
        self.assertIn('createResourceBarElements', content, "web/app.js must define createResourceBarElements")


class TestUsageBarsLogicInNode(unittest.TestCase):
    """Executes ESP32ControlPanel methods in Node.js with mock DOM to test rendering logic."""

    @classmethod
    def setUpClass(cls):
        # Verify node exists
        try:
            res = subprocess.run(["node", "--version"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
            cls.has_node = True
        except Exception:
            cls.has_node = False

    def run_node_test(self, js_test_code):
        if not self.has_node:
            self.skipTest("Node.js is not available in environment")

        node_script = f"""
const fs = require('fs');
const path = require('path');

// Minimal mock DOM
class MockElement {{
    constructor(id = '', tag = 'div') {{
        this.id = id;
        this.tagName = tag.toUpperCase();
        this.className = '';
        this.textContent = '';
        this.style = {{}};
        this.attributes = {{}};
        this.children = [];
        this.parentElement = null;
    }}
    setAttribute(name, val) {{ this.attributes[name] = String(val); }}
    getAttribute(name) {{ return this.attributes[name]; }}
    closest(sel) {{ return this.parentElement; }}
    appendChild(child) {{
        child.parentElement = this;
        this.children.push(child);
        return child;
    }}
    querySelector(sel) {{
        if (sel.startsWith('#')) {{
            const targetId = sel.slice(1);
            if (this.id === targetId) return this;
            for (const c of this.children) {{
                const found = c.querySelector(sel);
                if (found) return found;
            }}
        }}
        return null;
    }}
}}

const elements = {{
    'page-title': new MockElement('page-title'),
    'device-name': new MockElement('device-name'),
    'version': new MockElement('version'),
    'ip': new MockElement('ip'),
    'uptime': new MockElement('uptime'),
    'wifi-signal': new MockElement('wifi-signal'),
    'wifi-icon': new MockElement('wifi-icon'),
    'rssi-bar': new MockElement('rssi-bar'),
    'heap-bar': new MockElement('heap-bar'),
    'heap-usage': new MockElement('heap-usage'),
    'flash-bar': new MockElement('flash-bar'),
    'flash-usage': new MockElement('flash-usage'),
    'alert-status': new MockElement('alert-status'),
    'alert-controls': new MockElement('alert-controls'),
    'connection-status': new MockElement('connection-status'),
}};

const cardBody = new MockElement('card-body');
cardBody.appendChild(elements['wifi-signal']);
cardBody.appendChild(elements['rssi-bar']);

global.document = {{
    getElementById: (id) => elements[id] || null,
    querySelector: (sel) => null,
    createElement: (tag) => new MockElement('', tag)
}};

const {{ ESP32ControlPanel }} = require(path.join('{PROJECT_ROOT}', 'web', 'app.js'));

{js_test_code}
"""
        result = subprocess.run(
            ["node", "-e", node_script],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if result.returncode != 0:
            self.fail(f"Node execution failed with error:\n{result.stderr}\nOutput:\n{result.stdout}")
        return json.loads(result.stdout.strip())

    def test_rssi_bar_calculation_and_colors(self):
        js = """
const panel = new ESP32ControlPanel();

// Test 1: Excellent signal (-45 dBm -> 100% or > 80%, bg-success)
panel.deviceData = { wifi_rssi: -45 };
panel.updateRssiBar();
const r1 = {
    width: document.getElementById('rssi-bar').style.width,
    className: document.getElementById('rssi-bar').className,
    valuenow: document.getElementById('rssi-bar').getAttribute('aria-valuenow')
};

// Test 2: Good signal (-65 dBm -> 70%, bg-info)
panel.deviceData = { wifi_rssi: -65 };
panel.updateRssiBar();
const r2 = {
    width: document.getElementById('rssi-bar').style.width,
    className: document.getElementById('rssi-bar').className,
    valuenow: document.getElementById('rssi-bar').getAttribute('aria-valuenow')
};

// Test 3: Fair signal (-75 dBm -> 50%, bg-warning)
panel.deviceData = { wifi_rssi: -75 };
panel.updateRssiBar();
const r3 = {
    width: document.getElementById('rssi-bar').style.width,
    className: document.getElementById('rssi-bar').className,
    valuenow: document.getElementById('rssi-bar').getAttribute('aria-valuenow')
};

// Test 4: Poor signal (-90 dBm -> 20%, bg-danger)
panel.deviceData = { wifi_rssi: -90 };
panel.updateRssiBar();
const r4 = {
    width: document.getElementById('rssi-bar').style.width,
    className: document.getElementById('rssi-bar').className,
    valuenow: document.getElementById('rssi-bar').getAttribute('aria-valuenow')
};

console.log(JSON.stringify({ r1, r2, r3, r4 }));
"""
        out = self.run_node_test(js)
        self.assertEqual(out["r1"]["width"], "100%")
        self.assertIn("bg-success", out["r1"]["className"])

        self.assertEqual(out["r2"]["width"], "70%")
        self.assertIn("bg-info", out["r2"]["className"])

        self.assertEqual(out["r3"]["width"], "50%")
        self.assertIn("bg-warning", out["r3"]["className"])

        self.assertEqual(out["r4"]["width"], "20%")
        self.assertIn("bg-danger", out["r4"]["className"])

    def test_heap_bar_calculation_and_labels(self):
        js = """
const panel = new ESP32ControlPanel();

// 320 KB total heap, 160 KB free -> 50% used
panel.deviceData = {
    free_heap: 163840,
    total_heap: 327680
};
panel.updateHeapBar();
const r1 = {
    width: document.getElementById('heap-bar').style.width,
    usageText: document.getElementById('heap-usage').textContent,
    valuenow: document.getElementById('heap-bar').getAttribute('aria-valuenow')
};

// High memory pressure (90% used -> bg-danger)
panel.deviceData = {
    free_heap: 32768,
    total_heap: 327680
};
panel.updateHeapBar();
const r2 = {
    width: document.getElementById('heap-bar').style.width,
    className: document.getElementById('heap-bar').className,
    usageText: document.getElementById('heap-usage').textContent
};

console.log(JSON.stringify({ r1, r2 }));
"""
        out = self.run_node_test(js)
        self.assertEqual(out["r1"]["width"], "50%")
        self.assertEqual(out["r1"]["valuenow"], "50")
        self.assertIn("50%", out["r1"]["usageText"])

        self.assertEqual(out["r2"]["width"], "90%")
        self.assertIn("bg-danger", out["r2"]["className"])
        self.assertIn("90%", out["r2"]["usageText"])

    def test_flash_bar_calculation_and_labels(self):
        js = """
const panel = new ESP32ControlPanel();

// 4MB flash, 1MB sketch -> 25% used
panel.deviceData = {
    flash_size: 4194304,
    flash_used: 1048576
};
panel.updateFlashBar();
const r1 = {
    width: document.getElementById('flash-bar').style.width,
    className: document.getElementById('flash-bar').className,
    usageText: document.getElementById('flash-usage').textContent,
    valuenow: document.getElementById('flash-bar').getAttribute('aria-valuenow')
};

console.log(JSON.stringify({ r1 }));
"""
        out = self.run_node_test(js)
        self.assertEqual(out["r1"]["width"], "25%")
        self.assertEqual(out["r1"]["valuenow"], "25")
        self.assertIn("bg-info", out["r1"]["className"])
        self.assertIn("1.00 MB / 4.00 MB", out["r1"]["usageText"])
        self.assertIn("25%", out["r1"]["usageText"])

    def test_draw_usage_bars_wrapper(self):
        js = """
const panel = new ESP32ControlPanel();
panel.deviceData = {
    wifi_rssi: -60,
    free_heap: 200000,
    total_heap: 327680,
    flash_size: 4194304,
    flash_used: 1310720
};
const res = panel.drawUsageBars();
console.log(JSON.stringify({
    hasHeap: !!res.heap,
    hasFlash: !!res.flash,
    hasRssi: !!res.rssi,
    heapPct: res.heap.pct,
    rssiPct: res.rssi.pct,
    flashPct: res.flash.pct
}));
"""
        out = self.run_node_test(js)
        self.assertTrue(out["hasHeap"])
        self.assertTrue(out["hasFlash"])
        self.assertTrue(out["hasRssi"])
        self.assertEqual(out["rssiPct"], 80)
        self.assertEqual(out["heapPct"], 39)
        self.assertEqual(out["flashPct"], 31)


def main():
    print("🔍 ESP32 Smart Monitor - Usage Bars (Heap/Flash/RSSI) Validation")
    print("================================================================")
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(TestUsageBarsSourceIntegration)
    suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(TestUsageBarsLogicInNode))
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
