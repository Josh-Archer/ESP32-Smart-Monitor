from datetime import datetime, timedelta
import os
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
from scripts.sleep_utils import should_stay_awake

def test_top_of_hour_awake():
    now = datetime(2024, 1, 1, 10, 5)
    assert should_stay_awake(now, None)

def test_recent_update_awake():
    now = datetime(2024, 1, 1, 10, 30)
    update = now - timedelta(minutes=10)
    assert should_stay_awake(now, update)

def test_can_sleep():
    now = datetime(2024, 1, 1, 10, 30)
    update = now - timedelta(minutes=40)
    assert not should_stay_awake(now, update)
