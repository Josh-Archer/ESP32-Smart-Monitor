#!/usr/bin/env python3
"""QR setup payload and page tests.

The C++ builders (src/setup_qr.cpp) and the browser builders (web/setup.js)
must emit the same Wi-Fi, dashboard, and Home Assistant payloads. Payloads
never include MQTT or Wi-Fi credentials from the firmware.
"""

import gzip
import os
import re
import subprocess
import unittest

PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))

# Tab-separated protocol lines. Expected is OK or ERR plus a code.
CASES = [
    (
        "WIFI\tCafe\tsecret\tWPA\t0",
        "OK\tWIFI:T:WPA;S:Cafe;P:secret;H:false;;",
    ),
    (
        "WIFI\ta:b;c,d\"e\\f\tp:a;s,s\"w\\d\tWPA\t1",
        "OK\tWIFI:T:WPA;S:a\\:b\\;c\\,d\\\"e\\\\f;"
        "P:p\\:a\\;s\\,s\\\"w\\\\d;H:true;;",
    ),
    (
        "WIFI\tOpenNet\t\tWPA\t0",
        "OK\tWIFI:T:nopass;S:OpenNet;H:false;;",
    ),
    (
        "WIFI\tOpenNet\tignored\tnopass\t0",
        "OK\tWIFI:T:nopass;S:OpenNet;H:false;;",
    ),
    (
        "WIFI\tnet\tabc\tWEP\t0",
        "OK\tWIFI:T:WEP;S:net;P:abc;H:false;;",
    ),
    (
        "WIFI\tnet\tabc\twpa\t0",
        "OK\tWIFI:T:WPA;S:net;P:abc;H:false;;",
    ),
    (
        "WIFI\tnet\tabc\tfoo\t0",
        "OK\tWIFI:T:WPA;S:net;P:abc;H:false;;",
    ),
    ("WIFI\t\tsecret\tWPA\t0", "ERR\t-1"),
    ("WIFI\t" + ("a" * 33) + "\tsecret\tWPA\t0", "ERR\t-2"),
    ("WIFI\tnet\t" + ("p" * 64) + "\tWPA\t0", "ERR\t-2"),
    ("DASH\thttp://192.0.2.10/", "OK\thttp://192.0.2.10/"),
    ("DASH\thttps://example.com/setup", "OK\thttps://example.com/setup"),
    ("DASH\tjavascript:alert(1)", "ERR\t-1"),
    ("DASH\thttp://user:pass@192.0.2.10/", "ERR\t-1"),
    ("DASH\t", "ERR\t-1"),
    (
        "HA\tESP32 Poop Monitor\thttp://192.0.2.10/\t192.0.2.20\t1883\t"
        "homeassistant\tesp32_poop_monitor",
        "OK\tesp32mon://setup?v=1&name=ESP32%20Poop%20Monitor&"
        "url=http%3A%2F%2F192.0.2.10%2F&mqtt=192.0.2.20%3A1883&"
        "discovery=homeassistant&device=esp32_poop_monitor",
    ),
    (
        "HA\tpoop-monitor\t\t\t0\t\t",
        "OK\tesp32mon://setup?v=1&name=poop-monitor",
    ),
    (
        "HA\t\t\t192.0.2.20\t0\t\t",
        "OK\tesp32mon://setup?v=1&mqtt=192.0.2.20",
    ),
    ("HA\t\t\t\t0\t\t", "OK\tesp32mon://setup?v=1"),
    ("HA\t\tjavascript:alert(1)\t\t0\t\t", "ERR\t-1"),
    ("HA\t\t\thost:bad\t1883\t\t", "ERR\t-1"),
    ("HA\t\t\t192.0.2.20\t70000\t\t", "ERR\t-1"),
]


def _run(cmd, data):
    return subprocess.run(
        cmd,
        input=data.encode("utf-8"),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
        cwd=PROJECT_ROOT,
    )


class TestSetupQrPayloads(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        binary = os.path.join("/tmp", "test_setup_qr")
        compile_cmd = [
            "g++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I",
            os.path.join(PROJECT_ROOT, "src"),
            os.path.join(
                PROJECT_ROOT, "scripts", "native", "test_setup_qr.cpp"
            ),
            os.path.join(PROJECT_ROOT, "src", "setup_qr.cpp"),
            "-o",
            binary,
        ]
        built = subprocess.run(
            compile_cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if built.returncode != 0:
            raise RuntimeError(built.stderr.decode("utf-8", "replace"))
        cls.binary = binary

    def _stdin(self):
        return "".join(line + "\n" for line, _expected in CASES)

    def _outputs(self, cmd):
        result = _run(cmd, self._stdin())
        if result.returncode != 0:
            detail = result.stderr.decode("utf-8", "replace")
            self.fail(detail or "runner failed")
        lines = result.stdout.decode("utf-8").splitlines()
        self.assertEqual(len(lines), len(CASES))
        return lines

    def test_cpp_and_js_match_expected_payloads(self):
        cpp_lines = self._outputs([self.binary])
        node = r"""
const s = require('./web/setup.js');
const readline = require('readline');
const rl = readline.createInterface({input: process.stdin});
function emit(err, value) {
  if (err) {
    const code = err.code === 'too long' ? -2 : -1;
    process.stdout.write('ERR\t' + code + '\n');
    return;
  }
  process.stdout.write('OK\t' + value + '\n');
}
rl.on('close', () => process.exit(0));
rl.on('line', (line) => {
  if (!line) return;
  const f = line.split('\t');
  try {
    if (f[0] === 'WIFI' && f.length === 5) {
      emit(null, s.buildWifiQrPayload({
        ssid: f[1], password: f[2], auth: f[3], hidden: f[4] === '1'
      }));
    } else if (f[0] === 'DASH' && f.length === 2) {
      emit(null, s.buildDashboardQrPayload(f[1]));
    } else if (f[0] === 'HA' && f.length === 7) {
      emit(null, s.buildHaQrPayload({
        name: f[1], dashboardUrl: f[2], mqttHost: f[3],
        mqttPort: f[4], discovery: f[5], deviceId: f[6]
      }));
    } else {
      emit(new Error('invalid'));
    }
  } catch (err) {
    emit(err);
  }
});
"""
        js_lines = self._outputs(["node", "-e", node])
        triples = zip(CASES, cpp_lines, js_lines)
        for (raw, expected), cpp_line, js_line in triples:
            self.assertEqual(cpp_line, expected, raw)
            self.assertEqual(js_line, expected, raw)
            if raw.startswith("WIFI") and "\tnopass\t" in raw:
                self.assertNotIn("ignored", cpp_line)
            if raw.startswith("HA") and cpp_line.startswith("OK"):
                self.assertNotIn("password", cpp_line)
                self.assertNotIn("token", cpp_line)

    def test_qr_svg_renders_without_embedding_payload_text(self):
        script = r"""
const qrcode = require('./web/vendor/qrcode.js');
global.qrcode = qrcode;
const s = require('./web/setup.js');
const payload = s.buildWifiQrPayload({
  ssid: 'ExampleNet', password: 'unit-test-pass', auth: 'WPA', hidden: false
});
const svg = s.renderQrSvg(payload, 'Wi-Fi join code');
if (svg.indexOf('<svg') !== 0) process.exit(2);
if (svg.indexOf('unit-test-pass') !== -1) process.exit(3);
process.stdout.write('svg-ok\n');
"""
        result = _run(["node", "-e", script], "")
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertIn(b"svg-ok", result.stdout)

    def test_js_rejects_control_characters(self):
        script = r"""
const s = require('./web/setup.js');
try {
  s.buildWifiQrPayload({
    ssid: 'bad\nssid', password: 'x', auth: 'WPA', hidden: false
  });
  process.exit(2);
} catch (err) {
  if (err.code !== 'invalid') process.exit(3);
}
process.stdout.write('ok\n');
"""
        result = _run(["node", "-e", script], "")
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertIn(b"ok", result.stdout)


class TestSetupQrSources(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        def read(path):
            full = os.path.join(PROJECT_ROOT, path)
            with open(full, encoding="utf-8") as handle:
                return handle.read()

        cls.web_server = read("src/web_server.cpp")
        cls.setup_js = read("web/setup.js")
        cls.setup_html = read("web/setup.html")
        cls.roadmap = read("ROADMAP.md")
        cls.ini = read("platformio.ini")

    def test_routes_and_page_are_wired(self):
        self.assertIn('server.on("/setup", handleSetup);', self.web_server)
        self.assertIn(
            'server.on("/setup/info", handleSetupInfo);',
            self.web_server,
        )
        self.assertIn("SETUP_PAGE_GZ", self.web_server)
        self.assertIn("setup_qr_build_ha", self.web_server)
        self.assertIn('href=\'/setup\'', self.web_server)
        self.assertIn('src="/setup.js"', self.setup_html)
        self.assertIn('src="/vendor/qrcode.js"', self.setup_html)
        self.assertIn("buildWifiQrPayload", self.setup_js)
        self.assertIn("pre:scripts/embed_setup_page.py", self.ini)
        self.assertIn("**Done (#42)**", self.roadmap)

    def test_firmware_handler_does_not_read_secrets(self):
        start = self.web_server.index("void handleSetupInfo()")
        end = self.web_server.index("void handleNotFound()")
        body = self.web_server[start:end]
        for banned in (
            "WIFI_PASSWORD",
            "MQTT_PASSWORD",
            "MQTT_USER",
            "mqttPassword",
            "mqttUser",
            "pushoverToken",
            "pushoverUser",
            "OTA_PASSWORD",
            "passwordSecondary",
        ):
            self.assertNotIn(banned, body)

    def test_page_does_not_upload_or_store_the_wifi_password(self):
        self.assertNotIn("localStorage", self.setup_js)
        self.assertNotIn("sessionStorage", self.setup_js)
        self.assertNotIn("POST", self.setup_js)
        self.assertIn("'/setup/info'", self.setup_js)
        self.assertEqual(len(re.findall(r"\bfetch\s*\(", self.setup_js)), 1)
        self.assertNotIn("YOUR_WIFI_PASSWORD", self.setup_html)
        self.assertNotIn("YOUR_MQTT_PASSWORD", self.setup_js)

    def test_embedded_page_roundtrip_has_no_secrets(self):
        import importlib.util

        path = os.path.join(PROJECT_ROOT, "scripts", "embed_setup_page.py")
        spec = importlib.util.spec_from_file_location("embed_setup_page", path)
        embed = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(embed)
        html = embed.build_setup_page_html(PROJECT_ROOT)
        self.assertIn(b"Join Wi-Fi", html)
        self.assertIn(b"Home Assistant", html)
        self.assertIn(b"buildWifiQrPayload", html)
        self.assertNotIn(b'src="/setup.js"', html)
        for banned in (
            b"WIFI_PASSWORD",
            b"MQTT_PASSWORD",
            b"YOUR_WIFI_PASSWORD",
            b"YOUR_MQTT_PASSWORD",
            b"unit-test-pass",
        ):
            # unit-test-pass is the self-test sample and is allowed in the
            # script source. Real credential macros are not.
            if banned == b"unit-test-pass":
                self.assertIn(banned, html)
            else:
                self.assertNotIn(banned, html)
        self.assertEqual(gzip.decompress(embed.gzip_page(html)), html)


if __name__ == "__main__":
    unittest.main()
