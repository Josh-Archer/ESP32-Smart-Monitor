#!/usr/bin/env python3
import datetime
import subprocess
import sys
from sleep_utils import should_stay_awake

DEVICE_HOST = "poop-monitor.local"

def main():
    now = datetime.datetime.now()
    try:
        subprocess.check_output(["ping", "-c", "1", "-W", "1", DEVICE_HOST], stderr=subprocess.DEVNULL)
        reachable = True
    except subprocess.CalledProcessError:
        reachable = False

    if not reachable and not should_stay_awake(now, None):
        print(f"{DEVICE_HOST} is not reachable and it may be sleeping.")
        return 1
    return 0

if __name__ == "__main__":
    sys.exit(main())
