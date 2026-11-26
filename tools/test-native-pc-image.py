#!/usr/bin/env python3
"""Reuse original disposable PC/QEMU tests for a 1 GiB native GUI development image.

Silent WAV elapsed and OCR are software smoke checks, not physical audio quality
or real Atom performance. Downloadable source image is never modified.
"""
import argparse
import importlib.util
import json
import tempfile
from pathlib import Path

spec = importlib.util.spec_from_file_location("original_pc_smoke", Path(__file__).with_name("test-pc-image.py"))
reference = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reference)


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
        with reference.Guest(args.arch, work, image, music, evidence, "native-bios-offline", memory_mib=1024) as guest:
            reference.wait_for("native GUI showing USB album without a NIC (1 GiB guest)", guest.offline_track_visible, timeout=420)
        with reference.Guest(args.arch, work, image, music, evidence, "native-uefi", uefi=True, lan=True, memory_mib=1024) as guest:
            reference.verify_uefi(guest)
            reference.wait_for("native GUI with UEFI", guest.offline_track_visible, timeout=180)
            status = guest.api("system/update/status")
            assert status.get("supported") is False and status.get("repository") == "BASF2HD/PiTunes-Native", status
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
        "boundedDisplayFailure": True, "fatDiagnosticReport": True,
        "performanceAcceptance": "not measured", "audioQuality": "not measured",
    }, indent=2) + "\n")


if __name__ == "__main__": main()
