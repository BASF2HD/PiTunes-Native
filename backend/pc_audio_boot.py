#!/usr/bin/env python3
"""Resolve the automatic PC output before MPD starts."""
import os
from pathlib import Path
import re
import tempfile

from audio_output import _proc_pcm_devices

AUTO_DEVICE = re.compile(r'(?m)(^        # PiTunes automatic playback device\n        device +)".*"$')
DEVICE = re.compile(r"plughw:(?:CARD=[A-Za-z0-9_]+,DEV=\d+|\d+,\d+)")


def preferred_pcm(devices):
    """Prefer a built-in analogue PCM, leaving arbitrary USB/HDMI routing alone."""
    for device in devices:
        description = device["device_desc"].lower()
        card = device["card_id"].lower()
        if "analog" not in description or "usb" in description or "usb" in card:
            continue
        short = device["short_name"]
        pcm = f"plughw:CARD={short},DEV={device['device']}"
        if not DEVICE.fullmatch(pcm):
            pcm = f"plughw:{device['card']},{device['device']}"
        return pcm
    return "default"


def select_default(config=Path("/etc/mpd.conf")):
    old = config.read_text()
    if not AUTO_DEVICE.search(old):
        return
    pcm = preferred_pcm(_proc_pcm_devices())
    new = AUTO_DEVICE.sub(lambda m: m[1] + f'"{pcm}"', old, count=1)
    if new == old:
        return
    with tempfile.NamedTemporaryFile(mode="w", dir=config.parent, delete=False) as output:
        try:
            path = Path(output.name)
            output.write(new)
            output.flush()
            os.fchmod(output.fileno(), config.stat().st_mode & 0o777)
            path.replace(config)
        finally:
            path.unlink(missing_ok=True)
    print(f"MPD automatic PCM: {pcm}")


if __name__ == "__main__":
    select_default()
