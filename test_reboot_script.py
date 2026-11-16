#!/usr/bin/env python3
"""
Tests for reboot_esp32.sh
=========================

The MQTT-only build has no web server, so the script reboots the device by
publishing to the firmware's reboot command topic and watching its
availability topic. These tests run the script against stand-in mosquitto
clients and lock in:

1. The topics in the script are the ones the firmware uses.
2. The reboot command goes to the broker named in src/config.cpp.
3. The broker password never appears on a command line.
4. Exit code 0 only when the device goes offline and comes back; 2 when the
   command was sent but the device was not seen coming back; 1 when the
   command could not be sent.
"""

import os
import re
import stat
import subprocess
import tempfile
import textwrap
import unittest
import uuid

PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.join(PROJECT_ROOT, "reboot_esp32.sh")
MQTT_MANAGER = os.path.join(PROJECT_ROOT, "src", "mqtt_manager.cpp")
CONFIG_CPP = os.path.join(PROJECT_ROOT, "src", "config.cpp")

# A throwaway value made up for each run; the tests only check where it ends up.
PASSWORD = f"placeholder-{uuid.uuid4().hex}"

FAKE_PUB = """\
#!/bin/bash
# Stand-in for mosquitto_pub: record how it was called, then signal the
# stand-in subscriber that the reboot command arrived.
printf '%s\\n' "$*" >> "$FAKE_DIR/pub.argv"
cp "$XDG_CONFIG_HOME/mosquitto_pub" "$FAKE_DIR/pub.options" 2>/dev/null
[ "${FAKE_PUB_FAILS:-}" = "1" ] && { echo "Connection refused" >&2; exit 5; }
touch "$FAKE_DIR/published"
exit 0
"""

FAKE_SUB = """\
#!/bin/bash
# Stand-in for mosquitto_sub -v: print the retained state, then, once the
# reboot command has been published, the transitions FAKE_SEQUENCE lists.
printf '%s\\n' "$*" >> "$FAKE_DIR/sub.argv"
topic=""
while [ $# -gt 0 ]; do [ "$1" = "-t" ] && topic="$2"; shift; done
echo "$topic online"
for _ in $(seq 1 200); do [ -e "$FAKE_DIR/published" ] && break; sleep 0.05; done
for state in ${FAKE_SEQUENCE:-}; do sleep 0.2; echo "$topic $state"; done
sleep 30
"""


def firmware_constant(name: str) -> str:
    with open(MQTT_MANAGER, encoding="utf-8") as handle:
        match = re.search(rf'const char\* {name} = "([^"]+)";', handle.read())
    assert match, name
    return match.group(1)


def script_constant(name: str) -> str:
    with open(SCRIPT, encoding="utf-8") as handle:
        match = re.search(rf'^{name}="([^"]+)"$', handle.read(), re.M)
    assert match, name
    return match.group(1)


class RebootScriptTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.fake_dir = self.tmp.name
        bin_dir = os.path.join(self.fake_dir, "bin")
        os.mkdir(bin_dir)
        for name, body in (("mosquitto_pub", FAKE_PUB), ("mosquitto_sub", FAKE_SUB)):
            path = os.path.join(bin_dir, name)
            with open(path, "w", encoding="utf-8") as handle:
                handle.write(body)
            os.chmod(path, os.stat(path).st_mode | stat.S_IXUSR)
        self.bin_dir = bin_dir

    def tearDown(self):
        self.tmp.cleanup()

    def run_script(self, *args, sequence="", pub_fails=False, env=None):
        environment = {
            "PATH": self.bin_dir + os.pathsep + os.environ["PATH"],
            "HOME": self.fake_dir,
            "FAKE_DIR": self.fake_dir,
            "FAKE_SEQUENCE": sequence,
            "FAKE_PUB_FAILS": "1" if pub_fails else "",
            "MQTT_USER": "mqtt-user",
            "MQTT_PASSWORD": PASSWORD,
        }
        environment.update(env or {})
        return subprocess.run(
            ["bash", SCRIPT, "--force", "--method", "mqtt", *args],
            capture_output=True, text=True, env=environment, timeout=60,
        )

    def read(self, name):
        with open(os.path.join(self.fake_dir, name), encoding="utf-8") as handle:
            return handle.read()

    def test_script_parses(self):
        subprocess.run(["bash", "-n", SCRIPT], check=True)

    def test_topics_match_the_firmware(self):
        self.assertEqual(firmware_constant("MQTT_COMMAND_TOPIC"), script_constant("MQTT_COMMAND_TOPIC"))
        self.assertEqual(
            firmware_constant("MQTT_AVAILABILITY_TOPIC"), script_constant("MQTT_AVAILABILITY_TOPIC")
        )
        with open(MQTT_MANAGER, encoding="utf-8") as handle:
            source = handle.read()
        # The firmware subscribes to <command topic>/reboot and acts on any payload.
        self.assertIn('mqttClient.subscribe((String(MQTT_COMMAND_TOPIC) + "/reboot").c_str());', source)
        self.assertIn('if (topicStr == String(MQTT_COMMAND_TOPIC) + "/reboot")', source)

    def test_reboot_is_published_and_the_device_is_seen_coming_back(self):
        result = self.run_script("--timeout", "10", sequence="offline online")
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        argv = self.read("pub.argv")
        self.assertIn(f"-t {firmware_constant('MQTT_COMMAND_TOPIC')}/reboot", argv)
        self.assertIn(f"-t {firmware_constant('MQTT_AVAILABILITY_TOPIC')}", self.read("sub.argv"))
        self.assertIn("Device went offline", result.stdout)
        self.assertIn("Device is back online", result.stdout)

    def test_broker_comes_from_config_cpp_and_the_password_stays_off_argv(self):
        result = self.run_script("--timeout", "10", sequence="offline online")
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        with open(CONFIG_CPP, encoding="utf-8") as handle:
            config = handle.read()
        host = re.search(r'const char\* mqttServer = "([^"]+)";', config).group(1)
        port = re.search(r"const int mqttPort = (\d+);", config).group(1)
        options = self.read("pub.options").splitlines()
        self.assertEqual([f"-h {host}", f"-p {port}", "-u mqtt-user", f"-P {PASSWORD}"], options)
        for recorded in ("pub.argv", "sub.argv"):
            self.assertNotIn(PASSWORD, self.read(recorded))
            self.assertNotIn("mqtt-user", self.read(recorded))
        self.assertNotIn(PASSWORD, result.stdout + result.stderr)

    def test_environment_overrides_the_broker(self):
        result = self.run_script(
            "--timeout", "10", sequence="offline online",
            env={"MQTT_HOST": "broker.example", "MQTT_PORT": "8883", "MQTT_USER": "", "MQTT_PASSWORD": ""},
        )
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        # No user: no credentials are written at all.
        self.assertEqual(["-h broker.example", "-p 8883"], self.read("pub.options").splitlines())

    def test_a_device_that_never_goes_offline_is_a_failure(self):
        result = self.run_script("--timeout", "3", sequence="")
        self.assertEqual(2, result.returncode, result.stdout + result.stderr)
        self.assertIn("did not report going offline", result.stdout)

    def test_a_device_that_does_not_come_back_is_a_failure(self):
        result = self.run_script("--timeout", "3", sequence="offline")
        self.assertEqual(2, result.returncode, result.stdout + result.stderr)
        self.assertIn("did not come back", result.stdout)

    def test_the_retained_online_state_is_not_taken_for_the_reboot(self):
        # "online" was already there before the command; only a new one counts.
        result = self.run_script("--timeout", "3", sequence="")
        self.assertNotIn("Device is back online", result.stdout)

    def test_a_publish_failure_is_reported(self):
        result = self.run_script("--timeout", "3", pub_fails=True)
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertIn("Could not publish", result.stdout)

    def test_no_wait_only_publishes(self):
        result = self.run_script("--no-wait")
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertFalse(os.path.exists(os.path.join(self.fake_dir, "sub.argv")))

    def test_settings_are_read_from_both_source_styles(self):
        """credentials.cpp defines `const char* NAME = "..."`, credentials.h may
        use `#define NAME "..."`; the script reads either."""
        with open(SCRIPT, encoding="utf-8") as handle:
            body = handle.read()
        function = body[body.index("read_setting() {"):body.index("CONFIG_CPP=")]
        with tempfile.TemporaryDirectory() as tmp:
            cpp = os.path.join(tmp, "a.cpp")
            header = os.path.join(tmp, "a.h")
            with open(cpp, "w", encoding="utf-8") as handle:
                handle.write('extern const char* OTHER;\nconst char* SETTING_A = "from-cpp";\n')
            with open(header, "w", encoding="utf-8") as handle:
                handle.write('#pragma once\n#define SETTING_B   "from-header"\n')
            probe = textwrap.dedent(f"""
                {function}
                read_setting SETTING_A "{cpp}" "{header}"; echo
                read_setting SETTING_B "{cpp}" "{header}"; echo
                read_setting MISSING "{cpp}" "{header}" || echo none
            """)
            out = subprocess.run(["bash", "-c", probe], capture_output=True, text=True, check=True)
        self.assertEqual(["from-cpp", "from-header", "none"], out.stdout.split())


if __name__ == "__main__":
    unittest.main(verbosity=2)
