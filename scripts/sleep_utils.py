#!/usr/bin/env python3
from datetime import datetime, timedelta

OTA_WINDOW_MINUTES = 15
NO_SLEEP_AFTER_UPDATE_MINUTES = 30

def should_stay_awake(now: datetime, firmware_update_time: datetime | None) -> bool:
    """Return True if device should avoid sleeping at the given time."""
    if now.minute < OTA_WINDOW_MINUTES:
        return True
    if firmware_update_time and (now - firmware_update_time) < timedelta(minutes=NO_SLEEP_AFTER_UPDATE_MINUTES):
        return True
    return False
