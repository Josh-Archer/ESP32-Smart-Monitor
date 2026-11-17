#!/usr/bin/env python3
"""
Tests for upload_http_ota.sh result handling (#57)
==================================================

The device writes the image and restarts before it answers the POST, so the
upload often ends with no HTTP status (curl reports 000). These tests run the
script in a scratch copy of the project with stand-in `pio`, `curl` and
`sleep` commands and check that:

1. A dropped upload followed by the device's OTA server answering again is a
   success (exit 0), with the version reported as not verified.
2. A dropped upload after which the device never comes back is a failure.
3. A 401 is still a failure that names the password.
"""

import os
import shutil
import stat
import subprocess
import tempfile
import unittest

PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))

FAKE_PIO = """#!/bin/bash
# Stand-in for `pio run --environment ENV`: write a small firmware image.
env=""
while [ $# -gt 0 ]; do [ "$1" = "--environment" ] && env="$2"; shift; done
mkdir -p ".pio/build/$env"
head -c 4096 /dev/zero > ".pio/build/$env/firmware.bin"
exit 0
"""

FAKE_CURL = """#!/bin/bash
# Stand-in for curl. FAKE_UPLOAD_CODE is what the POST to /update reports;
# FAKE_DEVICE_BACK=1 makes the OTA server answer (404) afterwards.
args="$*"
out=""
while [ $# -gt 0 ]; do
  case "$1" in -o) out="$2"; shift ;; esac
  shift
done
case "$args" in
  *telnet://*) exit 7 ;;
  *"/status"*) exit 7 ;;
  *"-X POST"*"/update"*)
    [ -n "$out" ] && printf 'device says hi' > "$out"
    printf '%s' "${FAKE_UPLOAD_CODE:-200}"
    [ "${FAKE_UPLOAD_CODE:-200}" = "000" ] && exit 28
    exit 0 ;;
  *":8267/"*)
    if [ "${FAKE_DEVICE_BACK:-}" = "1" ]; then printf '404'; exit 0; fi
    printf '000'; exit 7 ;;
esac
exit 0
"""


class UploadScriptResultTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.work = os.path.join(self.tmp.name, "project")
        os.mkdir(self.work)
        for name in ("upload_http_ota.sh", "platformio.ini"):
            shutil.copy(os.path.join(PROJECT_ROOT, name), self.work)
        shutil.copytree(os.path.join(PROJECT_ROOT, "scripts"), os.path.join(self.work, "scripts"))
        os.makedirs(os.path.join(self.work, ".venv", "bin"))
        self.bin = os.path.join(self.tmp.name, "bin")
        os.mkdir(self.bin)
        self._write(os.path.join(self.work, ".venv", "bin", "pio"), FAKE_PIO)
        self._write(os.path.join(self.bin, "curl"), FAKE_CURL)
        self._write(os.path.join(self.bin, "sleep"), "#!/bin/sh\nexit 0\n")

    def tearDown(self):
        self.tmp.cleanup()

    @staticmethod
    def _write(path, body):
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(body)
        os.chmod(path, os.stat(path).st_mode | stat.S_IXUSR)

    def run_script(self, upload_code, device_back):
        env = {
            "PATH": self.bin + os.pathsep + os.environ["PATH"],
            "HOME": self.tmp.name,
            "OTA_HOST": "192.0.2.10",
            "OTA_PASSWORD": "placeholder",
            "FAKE_UPLOAD_CODE": upload_code,
            "FAKE_DEVICE_BACK": "1" if device_back else "",
        }
        return subprocess.run(
            ["bash", "upload_http_ota.sh", "mqtt"],
            cwd=self.work, capture_output=True, text=True, env=env, timeout=120,
        )

    def test_dropped_upload_then_device_back_is_success(self):
        result = self.run_script("000", device_back=True)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertIn("restarts before answering", result.stdout)
        self.assertIn("Version not verified", result.stdout)
        self.assertNotIn("Upload failed", result.stdout)

    def test_dropped_upload_device_never_back_is_failure(self):
        result = self.run_script("000", device_back=False)
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertIn("did not come back", result.stdout)

    def test_wrong_password_still_fails(self):
        result = self.run_script("401", device_back=True)
        self.assertEqual(1, result.returncode, result.stdout + result.stderr)
        self.assertIn("wrong OTA_PASSWORD", result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
