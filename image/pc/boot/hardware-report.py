#!/usr/bin/env python3
"""Export allowlisted hardware and boot observations to the current boot disk."""
from collections import Counter
from datetime import datetime, timezone
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

BOOT = Path("/boot/efi")
UNITS = (
    "lightdm.service", "pitunes-api.service", "mpd.service",
    "NetworkManager.service", "pitunes-mount.service", "nginx.service",
)
MPD_EVENTS = {
    "audio-output-open-failed": r"failed to open (?:ALSA|audio|.*audio output|.*ALSA device)",
    "device-busy": r"device or resource busy",
    "device-missing": r"no such (?:file or directory|device)",
    "permission-denied": r"permission denied",
    "pcm-underrun": r"underrun",
}
KERNEL_EVENTS = {
    "acpi-bios-error": r"ACPI BIOS Error",
    "acpi-pci-osc-already-exists": r"AE_ALREADY_EXISTS",
    "b43-ucode15-missing": r"failed to load b43(?:-open)?/ucode15\.fw \(-2\)",
    "firmware-load-failure": r"(?:failed to load|Direct firmware load for).+\.fw",
    "graphics-error": r"(?:\bdrm\b|\bi915\b|\bnouveau\b|\bradeon\b).*\b(?:failed|error)\b",
    "audio-driver-error": r"\b(?:snd|sof)[_-]\w+.*\b(?:failed|error)\b",
    "usb-reset-disconnect": r"\busb\s+\d+-\d+.*(?:reset|disconnect)",
}


def read(path, limit=1024, tail=False):
    try:
        with open(path, "rb") as file:
            if tail:
                file.seek(max(0, os.fstat(file.fileno()).st_size - limit))
            return file.read(limit).decode("utf-8", "replace").strip()
    except OSError:
        return ""


def run(*command, timeout=5):
    try:
        return subprocess.run(command, capture_output=True, text=True, check=False,
                              errors="replace", timeout=timeout, env={**os.environ, "LC_ALL": "C"}).stdout
    except (OSError, subprocess.TimeoutExpired):
        return ""


def safe_component(text):
    return text if re.fullmatch(r"[\w .,()@+/-]{1,96}", text, re.ASCII) else "unavailable"


def flags(text, patterns):
    return Counter({name: len(re.findall(pattern, text, re.I)) for name, pattern in patterns.items()})


def boot_partition():
    if not Path("/etc/pitunes-pc-image").is_file():
        raise RuntimeError("PC image marker is missing")
    root = run("findmnt", "-n", "-o", "MAJ:MIN", "--mountpoint", "/").strip()
    esp = run("findmnt", "-n", "-o", "MAJ:MIN,FSTYPE", "--mountpoint", str(BOOT)).split()
    if len(esp) != 2 or esp[1] != "vfat" or any(not re.fullmatch(r"\d+:\d+", d) for d in (root, esp[0])):
        raise RuntimeError("The boot disk FAT partition is unavailable")
    root_dev, esp_dev = (Path("/sys/dev/block", device).resolve(strict=True) for device in (root, esp[0]))
    if root_dev == esp_dev or root_dev.parent != esp_dev.parent or not (esp_dev / "partition").exists():
        raise RuntimeError("The FAT partition is not on the current root disk")
    return BOOT


def lines_for_bus(bus):
    for path in sorted(Path("/sys/bus", bus, "devices").glob("*")):
        vendor, device = ("vendor", "device") if bus == "pci" else ("idVendor", "idProduct")
        parts = [read(path / key).removeprefix("0x").lower() for key in (vendor, device)]
        if not all(re.fullmatch(r"[0-9a-f]{4}", part) for part in parts):
            continue
        if bus == "pci":
            cls = read(path / "class").removeprefix("0x")
            if not re.fullmatch(r"(?:01|02|03|04|0c)[0-9a-f]{4}", cls):
                continue
            driver = (path / "driver").resolve().name if (path / "driver").is_symlink() else "unbound"
            yield f"PCI {cls} {':'.join(parts)} driver={safe_component(driver)}"
        else:
            speed = read(path / "speed")
            if not re.fullmatch(r"[0-9.]+", speed):
                speed = "unknown"
            yield f"USB {':'.join(parts)} link_Mbit/s={speed}"


def collect():
    yield "PiTunes Native PC hardware observations"
    yield datetime.now(timezone.utc).isoformat(timespec="seconds")
    commit = read("/opt/pitunes/config/.install-commit")
    yield "Source: " + (commit if re.fullmatch(r"[a-f0-9]{40}", commit) else "unknown")
    yield "Kernel: " + safe_component(os.uname().release)
    yield "Architecture: " + safe_component(os.uname().machine)
    yield "Boot mode: " + ("UEFI" if Path("/sys/firmware/efi").exists() else "BIOS")
    for key in ("sys_vendor", "product_name", "bios_version"):
        yield f"{key}: {safe_component(read(Path('/sys/class/dmi/id', key)))}"
    model = re.search(r"(?m)^model name\s*:\s*(.+)", read("/proc/cpuinfo", 4096))
    if model:
        yield "CPU: " + safe_component(model[1])
    memory = re.search(r"(?m)^MemTotal:\s+(\d+) kB", read("/proc/meminfo"))
    if memory:
        yield "MemTotal KiB: " + memory[1]
    uptime = read("/proc/uptime").split()
    if uptime and re.fullmatch(r"\d+\.\d+", uptime[0]):
        yield "Uptime seconds: " + uptime[0]
    analyze = run("systemd-analyze", "time").splitlines()
    for line in analyze:
        if line.startswith(("Startup finished in ", "graphical.target reached after ")) and re.fullmatch(r"[\w .()+=@-]+", line):
            yield line

    yield "\nHardware and firmware messages (classified kernel journal counts)"
    for key, count in flags(run("journalctl", "-b", "-k", "--no-pager", "-o", "cat", "-n", "2500"),
                            KERNEL_EVENTS).items():
        yield f"{key}: {count}"
    for bus in ("pci", "usb"):
        yield from lines_for_bus(bus)
    yield "b43 loaded: " + str(Path("/sys/module/b43").exists())
    yield "b43 ucode15 file present: " + str(any(Path("/lib/firmware").glob("b43*/ucode15.fw*")))
    for rfkill in sorted(Path("/sys/class/rfkill").glob("rfkill*")):
        if read(rfkill / "type") == "wlan":
            blocks = [read(rfkill / name) for name in ("soft", "hard")]
            if all(block in ("0", "1") for block in blocks):
                yield f"Wi-Fi rfkill: soft={blocks[0]} hard={blocks[1]}"
    for device in sorted(Path("/sys/class/net").glob("*")):
        if (device / "phy80211").is_dir() or (device / "wireless").is_dir():
            yield "Wi-Fi interface: " + safe_component(device.name)
    wireless = run("nmcli", "-t", "radio", "wifi").strip()
    yield "NetworkManager Wi-Fi radio: " + (wireless if wireless in ("enabled", "disabled") else "unavailable")

    yield "\nALSA playback devices and selected mixer states"
    for line in run("aplay", "-l").splitlines():
        if not re.fullmatch(r"card \d+: [\w .,\[\]()+:/-]{1,160}", line, re.ASCII):
            continue
        yield line
    for card in sorted(Path("/proc/asound").glob("card[0-9]*")):
        index = card.name.removeprefix("card")
        if not index.isdigit():
            continue
        for control in ("Master", "Speaker", "Headphone", "Auto-Mute Mode", "PCM"):
            output = run("amixer", "-c", index, "sget", control, timeout=1)
            if not output:
                continue
            values = sorted(set(re.findall(r"\[(on|off)\]|Item0: '(Enabled|Disabled)'", output)))
            observed = "/".join(a or b for a, b in values)
            yield f"Card {index}: {control} {observed or 'present'}"

    yield "\nService state"
    for unit in UNITS:
        state = run("systemctl", "show", unit, "-p", "ActiveState", "-p", "SubState",
                    "-p", "ActiveEnterTimestampMonotonic", "-p", "Result")
        for field in state.splitlines():
            if re.fullmatch(r"(?:ActiveState|SubState|Result|ActiveEnterTimestampMonotonic)=[A-Za-z0-9_-]+", field):
                yield f"{unit} {field}"
    for line in run("systemctl", "--failed", "--no-legend", "--plain").splitlines():
        fields = line.split()
        if fields and re.fullmatch(r"(?:pitunes-[a-z-]+|mpd|lightdm|NetworkManager|nginx)\.(?:service|mount)", fields[0]):
            yield "Failed unit: " + fields[0]

    yield "\nLightDM journal and Xorg (classified counts)"
    display = run("journalctl", "-b", "-u", "lightdm.service", "-n", "250", "--no-pager", "-o", "cat")
    xorg = read("/var/log/Xorg.0.log", 16384, tail=True) + read("/var/log/lightdm/x-0.log", 16384, tail=True)
    yield f"LightDM errors: {len(re.findall(r'failed|error', display, re.I))}"
    yield f"Xorg error markers: {xorg.count('(EE)')}"
    yield "Xorg driver mentions: " + ", ".join(
        name for name in ("modesetting", "libinput", "evdev", "fbdev", "vesa", "intel", "synaptics") if name in xorg
    )
    yield "\nMPD journal and recent log (classified counts)"
    mpd = run("journalctl", "-b", "-u", "mpd.service", "-n", "250", "--no-pager", "-o", "cat")
    mpd += read("/var/log/mpd/mpd.log", 32768, tail=True)
    for key, count in flags(mpd, MPD_EVENTS).items():
        yield f"{key}: {count}"


def main():
    destination = boot_partition()
    name = "pitunes-display.log" if sys.argv[1:] == ["--display-failure"] else "pitunes-hardware.txt"
    report = "\n".join(collect()) + "\n"
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=destination, prefix=".pitunes-report-",
                                         delete=False) as file:
            temporary = Path(file.name)
            file.write(report)
            file.flush()
            os.fsync(file.fileno())
        temporary.replace(destination / name)
    finally:
        if temporary:
            temporary.unlink(missing_ok=True)
    print(f"Saved {name} on the PITUNES_EFI partition.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError) as error:
        sys.exit(str(error))
