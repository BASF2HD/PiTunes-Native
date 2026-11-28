#!/usr/bin/env python3
"""Reuse original disposable PC/QEMU tests for a 1 GiB native GUI development image.

Silent WAV elapsed and OCR are software smoke checks, not physical audio quality
or real Atom performance. Downloadable source image is never modified.
"""
import argparse
import importlib.util
import json
import tempfile
import urllib.request
from pathlib import Path

spec = importlib.util.spec_from_file_location("original_pc_smoke", Path(__file__).with_name("test-pc-image.py"))
reference = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reference)


def instrumented_disk(work, image):
    disk = work / "native-process-inspection.img"
    reference.run("cp", "--sparse=always", "--reflink=auto", image, disk)
    root = work / "probe-root"
    root.mkdir()
    unit = work / "native-boot-probe.service"
    unit.write_text(
        "[Unit]\nDescription=Record the native session in disposable QEMU tests\nAfter=lightdm.service\n"
        "[Service]\nType=oneshot\nExecStart=/usr/bin/python3 /var/tmp/native-boot-probe.py\n"
        "[Install]\nWantedBy=graphical.target\n",
        encoding="ascii",
    )
    loop = reference.run("sudo", "losetup", "--find", "--show", "--partscan", disk,
                         capture_output=True, text=True).stdout.strip()
    try:
        reference.run("sudo", "udevadm", "settle")
        reference.run("sudo", "mount", f"{loop}p2", root)
        try:
            reference.run("sudo", "install", "-m", "0644", Path(__file__).with_name("native-boot-probe.py"),
                          root / "var/tmp/native-boot-probe.py")
            reference.run("sudo", "install", "-m", "0644", unit, root / "etc/systemd/system" / unit.name)
            reference.run("sudo", "systemctl", "--root", root, "enable", unit.name)
        finally:
            reference.run("sudo", "umount", root)
    finally:
        reference.run("sudo", "losetup", "-d", loop)
    return disk


def verify_processes(guest, evidence):
    serial = evidence / (guest.name + ".serial.log")
    marker = "PITUNES_BOOT_AUDIT "

    def read_report():
        for line in serial.read_text(errors="replace").splitlines():
            if marker in line:
                try:
                    return json.loads(line.split(marker, 1)[1]) or False
                except json.JSONDecodeError:
                    continue
        return False

    report = reference.wait_for("native session process observations", read_report, timeout=135)
    (evidence / (guest.name + ".processes.json")).write_text(json.dumps(report, indent=2) + "\n")
    assert len(report) == 5, report
    for sample in report:
        assert sample["legacyDisplayMasked"] and not sample["browserProcesses"], sample
        assert sample["windowManagers"] == 1 and len(sample["players"]) == 1, sample
        player = sample["players"][0]
        assert player["uid"] > 0 and player["fullscreen"], player
    assert len({sample["players"][0]["pid"] for sample in report}) == 1, report


def verify_hardware_report(guest, evidence):
    serial = evidence / (guest.name + ".serial.log")
    marker = "PITUNES_HARDWARE_AUDIT "

    def read_report():
        for line in serial.read_text(errors="replace").splitlines():
            if marker in line:
                try:
                    return json.loads(line.split(marker, 1)[1])
                except json.JSONDecodeError:
                    continue
        return False

    audit = reference.wait_for("routine boot hardware report", read_report, timeout=170)
    if not audit["supported"]:
        return False
    assert "report" in audit, audit.get("error")
    report = audit["report"]
    assert "Source: " in report and "Source: unknown" not in report
    assert "ALSA playback devices" in report and "MPD journal and recent log (classified counts)" in report
    assert reference.TRACK not in report and "BEGIN OPENSSH" not in report
    (evidence / "pitunes-hardware.txt").write_text(report)
    return True


def verify_detected_audio_route(guest):
    audio = guest.api("audio/devices")
    if audio.get("platform") != "pc":
        return False
    analog = next(d for d in audio["devices"] if d.get("kind") == "analog" or "analog" in d["label"].lower())
    assert analog["alsa"].startswith("plughw:CARD="), analog
    applied = guest.api("audio/output", {"output": "pc-alsa", "alsa": analog["alsa"], "mixer": "software"}, timeout=40)
    assert applied.get("ok") is True, applied
    reference.verify_uefi(guest)
    return True


def verify_remote_frontend(guest):
    for path, expected_type in (("/", "text/html"), ("/assets/app.js", "application/javascript"),
                                ("/assets/styles.css", "text/css")):
        with urllib.request.urlopen(f"http://127.0.0.1:{guest.port}{path}", timeout=10) as response:
            actual_type = response.headers.get_content_type()
            assert actual_type == expected_type or (path.endswith(".js") and actual_type == "text/javascript"), actual_type
            assert response.read(256).strip(), path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("arch", choices=("amd64", "i386"))
    parser.add_argument("image", type=Path)
    parser.add_argument("evidence", type=Path)
    args = parser.parse_args()
    image = args.image.resolve(strict=True)
    evidence = args.evidence.resolve(); evidence.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="native-pc-smoke-") as temporary:
        work = Path(temporary); music = reference.music_disk(work)
        observed = instrumented_disk(work, image)
        with reference.Guest(args.arch, work, observed, music, evidence, "native-bios-offline", memory_mib=1024) as guest:
            reference.wait_for("native GUI showing USB album without a NIC (1 GiB guest)", guest.offline_track_visible, timeout=420)
            verify_processes(guest, evidence)
        with reference.Guest(args.arch, work, observed, music, evidence, "native-uefi", uefi=True, lan=True, memory_mib=1024) as guest:
            reference.verify_uefi(guest)
            reference.wait_for("native GUI with UEFI", guest.offline_track_visible, timeout=180)
            status = guest.api("system/update/status")
            assert status.get("supported") is False and status.get("repository") == "BASF2HD/PiTunes-Native", status
            verify_processes(guest, evidence)
            verify_remote_frontend(guest)
            verified_hardware_report = verify_hardware_report(guest, evidence)
            verified_audio_route = verify_detected_audio_route(guest)
        fault = reference.display_fault_disk(work, image)
        with reference.Guest(args.arch, work, fault, music, evidence, "native-display-fault", uefi=True,
                             lan=True, memory_mib=1024, save_overlay=True) as guest:
            reference.wait_for("API while Xorg fails", lambda: guest.api("health").get("ok"))
            def display_failed():
                services = guest.api("services", timeout=35).get("services", {})
                kiosk = services.get("kiosk", [{}])[0]
                return any(c.get("unit") == "lightdm.service" and c.get("active") == "failed"
                           for c in kiosk.get("components", ()))
            reference.wait_for("bounded LightDM failure", display_failed)
            # Reuses the exact USB playback check while the display cannot start.
            reference.verify_uefi(guest)
            assert guest.api("health").get("ok"), "API failed with unavailable display"
            overlay = guest.overlay
        fat = work / "efi.img"
        reference.run("qemu-img", "dd", "-f", "qcow2", "bs=512", "skip=2048", "count=262144",
                      f"if={overlay}", f"of={fat}")
        report = evidence / "pitunes-display.log"
        reference.run("mcopy", "-i", fat, "::pitunes-display.log", report)
        assert "LightDM journal" in report.read_text(), "Display report is incomplete"
    (evidence / "native-smoke-result.json").write_text(json.dumps({
        "measurementKind": "virtual machine smoke test", "architecture": args.arch,
        "memoryMiB": 1024, "biosOfflineNativeGui": True, "uefiGuiAndSilentWavPlayback": True,
        "mpdPlaybackWithDisplayFailure": True, "physicalHardware": False,
        "routineHardwareReport": verified_hardware_report, "detectedAlsaRoutePlayback": verified_audio_route,
        "singleNativePlayerAfterLateBoot": True, "browserProcessesObserved": False, "legacyDisplayMasked": True,
        "remoteHtmlJsCssServedByNginx": True,
        "boundedDisplayFailure": True, "fatDiagnosticReport": True,
        "performanceAcceptance": "not measured", "audioQuality": "not measured",
    }, indent=2) + "\n")


if __name__ == "__main__": main()
