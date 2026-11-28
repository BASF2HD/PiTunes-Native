"""ALSA device discovery and audio output routing for PiTunes."""

from __future__ import annotations

import io
import json
import math
import os
import re
import subprocess
import wave
from array import array
from pathlib import Path
from typing import Any

PROJECT_ROOT = Path(__file__).resolve().parent.parent
HATS_FILE = PROJECT_ROOT / "config" / "dac-hats.json"
APPLY_SCRIPT = PROJECT_ROOT / "scripts" / "apply-audio-output.sh"

PC_IMAGE = Path("/etc/pitunes-pc-image")
VALID_ROUTES = frozenset({"usb-dac", "dac-hat", "hdmi", "headphones", "pc-alsa"})
CONTROL_NAME = re.compile(r"[ A-Za-z0-9_.+()-]{1,60}")

# Kernel/ALSA identifiers mapped to short labels when device text is too technical.
CARD_ALIASES = {
    "snd_rpi_hifiberry_dac": "HiFiBerry DAC",
    "snd_rpi_hifiberry_dacplus": "HiFiBerry DAC+",
    "snd_rpi_hifiberry_dacpluspro": "HiFiBerry DAC+ Pro",
    "snd_rpi_hifiberry_dacplushd": "HiFiBerry DAC+ HD",
    "snd_rpi_hifiberry_dac8x": "HiFiBerry DAC8x",
    "snd_rpi_hifiberry_amp": "HiFiBerry Amp",
    "snd_rpi_hifiberry_amp3": "HiFiBerry Amp3",
    "snd_rpi_hifiberry_amp4pro": "HiFiBerry Amp4 Pro",
    "snd_rpi_hifiberry_digi": "HiFiBerry Digi+",
    "snd_rpi_hifiberry_digipro": "HiFiBerry Digi+ Pro",
    "snd_rpi_googlevoicehat_soundcard": "Google Voice HAT",
    "bcm2835_headphones": "3.5 mm Headphones",
    "bcm2835 headphones": "3.5 mm Headphones",
    "vc4_hdmi": "HDMI",
    "vc4-hdmi": "HDMI",
    "sndrpihifiberry": "HiFiBerry DAC+",
}

DEVICE_ALIASES = {
    "bcm2835 headphones": "3.5 mm Headphones",
    "vc4-hdmi": "HDMI",
    "mai pcm": "HDMI",
}


def _clean_device_name(name: str) -> str:
    cleaned = name.strip()
    if "[" in cleaned:
        cleaned = cleaned.split("[", 1)[0].strip()
    cleaned = re.sub(r"\s+HiFi\s+\S+(?:-hifi-\d+)?\s*$", "", cleaned, flags=re.I).strip()
    cleaned = re.sub(r"\s+\S+-hifi-\d+\s*$", "", cleaned, flags=re.I).strip()
    return cleaned


def _friendly_device_label(short_name: str, card_id: str, device_desc: str, *, multi_card: bool, card: int) -> str:
    cleaned = _clean_device_name(device_desc)
    lowered = cleaned.lower()
    for needle, label in DEVICE_ALIASES.items():
        if needle in lowered:
            cleaned = label
            break

    card_key = (card_id or short_name or "").strip().lower().replace(" ", "_")
    if not cleaned or re.fullmatch(r"[a-z0-9_]+", cleaned.replace(" ", "").replace("-", "")):
        cleaned = CARD_ALIASES.get(card_key, CARD_ALIASES.get((short_name or "").strip().lower(), cleaned or card_id or short_name or "ALSA device"))

    if multi_card:
        return f"{cleaned} (card {card})"
    return cleaned


def _run_aplay_list() -> str:
    try:
        proc = subprocess.run(
            ["aplay", "-l"],
            capture_output=True,
            text=True,
            timeout=3.0,
            check=False,
            env={**os.environ, "LC_ALL": "C"},
        )
    except (OSError, subprocess.TimeoutExpired):
        return ""
    return proc.stdout or ""


def _proc_pcm_devices(root: Path = Path("/proc/asound")) -> list[dict[str, Any]]:
    """Read playback PCMs exposed by the kernel even if aplay cannot open ctl."""
    try:
        pcm_lines = (root / "pcm").read_text().splitlines()
    except OSError:
        return []
    try:
        cards = dict(re.findall(r"(?m)^\s*(\d+)\s+\[[^\]]+\]:\s*[^\n]*? - (.+)$",
                                (root / "cards").read_text()))
    except OSError:
        cards = {}
    result = []
    for line in pcm_lines:
        match = re.match(r"^(\d+)-(\d+):\s*([^:]+):.+:\s*playback\s+[1-9]", line)
        if not match:
            continue
        card, device, name = match.groups()
        number = int(card)
        try:
            short = (root / f"card{number}" / "id").read_text().strip()
        except OSError:
            short = ""
        result.append({"card": number, "device": int(device), "short_name": short,
                       "card_id": cards.get(str(number), short or f"Card {number}"),
                       "device_desc": name.strip()})
    return result


def list_alsa_devices(*, pc: bool = False) -> list[dict[str, Any]]:
    output = _run_aplay_list()
    devices: list[dict[str, Any]] = []
    pattern = re.compile(
        r"^card (\d+): ([^,]+?)(?: \[([^\]]*)\])?, device (\d+): (.+)$",
        re.I,
    )
    for line in output.splitlines():
        match = pattern.match(line.strip())
        if not match:
            continue
        card, short_name, card_id, device, device_desc = match.groups()
        devices.append(
            {
                "alsa": f"plughw:{card},{device}",
                "label": "",
                "card": int(card),
                "device": int(device),
                "short_name": (short_name or "").strip(),
                "card_id": (card_id or "").strip(),
                "device_desc": (device_desc or "").strip(),
            }
        )
    if pc:
        reported = {(d["card"], d["device"]) for d in devices}
        for entry in _proc_pcm_devices():
            if (entry["card"], entry["device"]) not in reported:
                devices.append(entry)
    multi_card = len(devices) > 1
    mixers = {}
    for entry in devices:
        short = entry.pop("short_name")
        card_name = entry.pop("card_id") or short
        pcm_name = entry.pop("device_desc")
        if pc:
            if re.fullmatch(r"[A-Za-z0-9_]+", short):
                entry["alsa"] = f"plughw:CARD={short},DEV={entry['device']}"
            else:
                entry["alsa"] = f"plughw:{entry['card']},{entry['device']}"
            entry["label"] = f"{card_name} · {_clean_device_name(pcm_name)}"
            entry["kind"] = "hdmi" if "hdmi" in pcm_name.lower() else "usb" if "usb" in (card_name + pcm_name).lower() else "analog" if "analog" in pcm_name.lower() else "other"
            if entry["kind"] == "analog":
                codec = _analogue_codec(entry["card"], pcm_name)
                entry["codec"] = codec
                entry["label"] = f"Built-in Audio ({codec})" if codec else f"Built-in Audio ({_clean_device_name(pcm_name)})"
            if entry["card"] not in mixers:
                mixers[entry["card"]] = list_mixer_controls(entry["card"])
            entry["controls"] = mixers[entry["card"]]
        else:
            entry["label"] = _friendly_device_label(short, card_name, pcm_name, multi_card=multi_card, card=entry["card"])
    if not devices and not pc:
        devices.append({"alsa": "default", "label": "default - ALSA default output", "card": -1, "device": 0})
    return devices


def _analogue_codec(card: int, pcm_name: str) -> str:
    codecs = []
    for codec in sorted(Path(f"/proc/asound/card{card}").glob("codec#*")):
        try:
            with codec.open() as stream:
                heading = stream.readline(160).strip()
        except OSError:
            continue
        if heading.startswith("Codec: ") and "HDMI" not in heading.upper():
            codecs.append(heading.removeprefix("Codec: "))
    return next((c for c in codecs if c.split()[-1] in pcm_name), codecs[0] if codecs else "")


def list_mixer_controls(card: int) -> list[dict[str, Any]]:
    try:
        output = subprocess.run(
            ["amixer", "-c", str(card), "scontents"], capture_output=True, text=True, timeout=2,
            env={**os.environ, "LC_ALL": "C"}, check=False,
        ).stdout
    except (OSError, subprocess.TimeoutExpired):
        return []
    return parse_mixer_controls(output)


def parse_mixer_controls(output: str) -> list[dict[str, Any]]:
    controls = []
    for block in re.split(r"(?m)^Simple mixer control ", output)[1:]:
        heading, _, body = block.partition("\n")
        match = re.fullmatch(r"'([^']+)',0", heading)
        if not match or not CONTROL_NAME.fullmatch(match[1]):
            continue
        capability = re.search(r"(?m)^\s*Capabilities: (.*)$", body)
        if not capability:
            continue
        flags = capability[1].split()
        label = match[1]
        volume = "pvolume" in flags
        switch = "pswitch" in flags
        port = switch and label in {"Speaker", "Headphone"}
        if volume or switch or label == "Auto-Mute Mode":
            values = re.findall(r"\[(on|off)\]", body)
            enum = re.search(r"(?m)^\s*Item0: '([^']+)'", body)
            state = ("/".join(dict.fromkeys(values)) if values else
                     enum[1] if enum and enum[1] in ("Enabled", "Disabled") else "")
            percentages = re.findall(r"(?m)^.+Playback[^\n]*?\[(\d+)%\]", body)
            controls.append({"name": label, "volume": volume, "port": port, "state": state,
                             "percent": int(percentages[0]) if percentages else None})
    return controls


def load_dac_hats() -> list[dict[str, Any]]:
    try:
        data = json.loads(HATS_FILE.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return []
    hats = data.get("hats") or []
    return [
        {
            "id": hat.get("id", ""),
            "label": hat.get("label", hat.get("id", "")),
            "overlay": hat.get("overlay", ""),
            "mixer": hat.get("mixer", "hardware"),
        }
        for hat in hats
        if hat.get("id")
    ]


def normalize_route(route: str | None) -> str:
    value = (route or "").strip().lower()
    if value in ("", "auto", "default"):
        return "hdmi"
    if value == "browser":
        return "browser"
    if value in VALID_ROUTES:
        return value
    return "hdmi"


def audio_devices_payload(settings: dict[str, Any]) -> dict[str, Any]:
    pc = PC_IMAGE.is_file()
    devices = list_alsa_devices(pc=pc)
    routes = [
        {"id": "hdmi", "label": "HDMI"},
        {"id": "headphones", "label": "3.5 mm Headphones"},
        {"id": "usb-dac", "label": "USB DAC"},
        {"id": "dac-hat", "label": "DAC HAT (I2S)"},
    ]
    return {
        "platform": "pc" if pc else "pi",
        "devices": ([{"alsa": "default", "label": "ALSA default", "card": -1, "device": 0}] + devices) if pc else devices,
        "hats": [] if pc else load_dac_hats(),
        "routes": ([{"id": "pc-alsa", "label": "ALSA playback"}] + routes[:-1]) if pc else routes,
        "current": {
            "route": normalize_route(settings.get("audio_output")),
            "device": settings.get("alsa_device") or "default",
            "mixer": settings.get("mixer") or "software",
            "mixer_control": settings.get("mixer_control") or "",
            "dac_hat": settings.get("dac_hat") or "",
        },
    }



def _selected_pc_device(alsa: str) -> dict[str, Any]:
    if not PC_IMAGE.is_file():
        raise ValueError("This control is available on PC images.")
    device = next((entry for entry in list_alsa_devices(pc=True) if entry["alsa"] == alsa), None)
    if not device:
        raise ValueError("Select a detected audio device first.")
    return device


def apply_pc_mixer(body: dict[str, Any]) -> dict[str, Any]:
    device = _selected_pc_device(body.get("alsa"))
    name = body.get("control")
    control = next((c for c in device["controls"] if c["name"] == name), None)
    if device.get("kind") != "analog" or not control:
        raise ValueError("The selected analogue output does not expose that control.")
    args = []
    if name == "Auto-Mute Mode" and control["state"] in ("Enabled", "Disabled"):
        action = body.get("state")
        if action in ("Enabled", "Disabled"):
            args.append(action)
    elif control["port"]:
        if "percent" in body:
            percent = body["percent"]
            if type(percent) is not int or not control["volume"] or not 0 <= percent <= 100:
                raise ValueError("Volume must be a supported percent from 0 to 100.")
            args.append(f"{percent}%")
        action = body.get("state")
        if action in ("mute", "unmute"):
            args.append(action)
    if not args:
        raise ValueError("Select a supported speaker, headphone or auto-mute setting.")
    _run_pc_audio(["amixer", "-q", "-c", str(device["card"]), "sset", name, *args])
    return {"ok": True, "message": f"{name} updated."}


def _test_clip() -> bytes:
    rate = 48000
    frames = rate // 2
    samples = array("h")
    for i in range(frames):
        ramp = min(1., i / 800., (frames - 1 - i) / 800.)
        value = round(819 * ramp * math.sin(2 * math.pi * 650 * i / rate))
        samples.extend((value, value))
    if os.sys.byteorder != "little":
        samples.byteswap()
    output = io.BytesIO()
    with wave.open(output, "wb") as tone:
        tone.setparams((2, 2, rate, frames, "NONE", "not compressed"))
        tone.writeframes(samples.tobytes())
    return output.getvalue()


def _run_pc_audio(command: list[str], data: bytes | None = None) -> None:
    try:
        result = subprocess.run(command, input=data, capture_output=True, timeout=4, check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise RuntimeError(f"Audio device could not be opened: {exc}") from exc
    if result.returncode:
        raise RuntimeError(result.stderr.decode(errors="replace").strip() or "The ALSA operation failed.")


def test_pc_audio(body: dict[str, Any]) -> dict[str, Any]:
    alsa = body.get("alsa")
    _selected_pc_device(alsa)
    _run_pc_audio(["aplay", "-q", "-D", alsa], _test_clip())
    return {"ok": True, "message": "Test tone sent to the selected output."}


def apply_audio_output(settings: dict[str, Any], body: dict[str, Any] | None = None) -> dict[str, Any]:
    body = body or {}
    route = normalize_route(body.get("output") or body.get("audio_output") or settings.get("audio_output"))
    if route == "browser":
        return {"ok": True, "message": "Browser output is handled on this device only.", "reboot_required": False}

    dac_hat = str(body.get("dac_hat") or settings.get("dac_hat") or "").strip()
    alsa_device = str(body.get("alsa") or body.get("alsa_device") or settings.get("alsa_device") or "default").strip()
    mixer = str(body.get("mixer") or settings.get("mixer") or "software").strip().lower()
    if mixer not in ("software", "hardware", "none"):
        mixer = "software"

    mixer_control = str(body.get("mixer_control") or "").strip()
    if route == "pc-alsa":
        if not PC_IMAGE.is_file():
            raise ValueError("PC audio is only available on PC images.")
        devices = list_alsa_devices(pc=True) if alsa_device != "default" else []
        selected = next((d for d in devices if d["alsa"] == alsa_device), {})
        if alsa_device != "default" and not selected:
            raise ValueError("The selected ALSA playback device is unavailable.")
        if mixer_control and (
            mixer != "hardware"
            or not any(c["name"] == mixer_control and c["volume"] for c in selected.get("controls", []))
        ):
            raise ValueError("The selected volume control is unavailable for this ALSA device.")

    if route == "dac-hat" and not dac_hat:
        raise ValueError("Select a DAC HAT model before applying this output route.")

    if not APPLY_SCRIPT.is_file():
        raise RuntimeError("Audio apply script is missing on this system.")

    try:
        proc = subprocess.run(
            [
                "sudo",
                "systemd-run",
                "--wait",
                "--pipe",
                "--collect",
                "--unit=pitunes-audio-apply",
                "/bin/bash",
                str(APPLY_SCRIPT),
                route,
                dac_hat,
                alsa_device,
                mixer,
                mixer_control,
            ],
            capture_output=True,
            text=True,
            timeout=30.0,
            check=False,
        )
    except Exception as exc:
        raise RuntimeError(f"Failed to apply audio output: {exc}") from exc

    stdout = (proc.stdout or "").strip()
    stderr = (proc.stderr or "").strip()
    if proc.returncode != 0:
        raise RuntimeError(stderr or stdout or "Audio output apply failed.")

    result: dict[str, Any] = {"ok": True, "message": f"Audio output set to {route}.", "reboot_required": False}
    if stdout:
        try:
            result.update(json.loads(stdout.splitlines()[-1]))
        except json.JSONDecodeError:
            result["message"] = stdout
    if result.get("reboot_required"):
        result["message"] = f"{result.get('message', 'Audio updated.')} Reboot the Pi to enable the DAC HAT."
    return result
