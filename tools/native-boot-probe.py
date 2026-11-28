#!/usr/bin/env python3
"""Read-only process evidence for the disposable QEMU smoke guest."""
import json
import os
import re
import subprocess
import time
from pathlib import Path

browser = re.compile(r"chromium|google-chrome|firefox|epiphany|electron|webkit|webengine|webview", re.I)


def snapshot():
    players = []
    wm_count = 0
    browsers = []
    for proc in Path("/proc").iterdir():
        if not proc.name.isdecimal():
            continue
        try:
            exe = Path(os.readlink(proc / "exe"))
            if browser.search(str(exe)):
                browsers.append(exe.name)
            if exe == Path("/usr/bin/pitunes-native"):
                players.append({
                    "pid": int(proc.name),
                    "uid": proc.stat().st_uid,
                    "fullscreen": b"--fullscreen" in (proc / "cmdline").read_bytes().split(b"\0"),
                })
            if exe.name == "openbox":
                wm_count += 1
        except (OSError, ValueError):
            continue
    mask = Path("/etc/systemd/system/pitunes-display.service")
    return {
        "players": players,
        "windowManagers": wm_count,
        "browserProcesses": sorted(browsers),
        "legacyDisplayMasked": mask.is_symlink() and mask.readlink() == Path("/dev/null"),
    }


def verify_hardware_report(serial):
    service = Path("/etc/systemd/system/pitunes-pc-hardware-report.service")
    if not service.exists():
        print("PITUNES_HARDWARE_AUDIT " + json.dumps({"supported": False}), file=serial, flush=True)
        return
    timeout = time.monotonic() + 120
    report = Path("/boot/efi/pitunes-hardware.txt")
    while time.monotonic() < timeout:
        try:
            body = report.read_text()
        except FileNotFoundError:
            time.sleep(2)
            continue
        print("PITUNES_HARDWARE_AUDIT " + json.dumps({"supported": True, "report": body}),
              file=serial, flush=True)
        return
    info = subprocess.run(
        ["systemctl", "status", "pitunes-pc-hardware-report.timer", "pitunes-pc-hardware-report.service"],
        capture_output=True, text=True, timeout=10, check=False,
    ).stdout
    print("PITUNES_HARDWARE_AUDIT " + json.dumps({"supported": True, "error": info}), file=serial, flush=True)


def main():
    # Let the inherited late-Samba timer fire before recording a stable session.
    time.sleep(20)
    deadline = time.monotonic() + 100
    samples = []
    while time.monotonic() < deadline:
        result = snapshot()
        if result["players"] or result["browserProcesses"]:
            samples.append(result)
            if len(samples) == 5:
                break
        time.sleep(2)
    with open("/dev/ttyS0", "w", encoding="ascii") as serial:
        print("PITUNES_BOOT_AUDIT " + json.dumps(samples, separators=(",", ":")), file=serial, flush=True)
        verify_hardware_report(serial)


if __name__ == "__main__":
    main()
