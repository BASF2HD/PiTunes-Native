import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from backend import audio_output, network_wifi


class AtomHardwareTests(unittest.TestCase):
    def test_hda_devices_use_card_identifiers_and_share_mixer_ports(self):
        listing = """card 0: PCH [HDA Intel PCH], device 0: ALC269VB Analog [ALC269VB Analog]
card 0: PCH [HDA Intel PCH], device 3: HDMI 0 [HDMI 0]
card 2: DAC [USB Audio DAC], device 0: USB Audio [USB Audio]"""
        mixer = """Simple mixer control 'Speaker',0
  Capabilities: pvolume pswitch
  Front Left: Playback 48 [65%] [on]
Simple mixer control 'Headphone',0
  Capabilities: pvolume pswitch
  Front Left: Playback 0 [0%] [off]
Simple mixer control 'Auto-Mute Mode',0
  Capabilities: enum
  Item0: 'Enabled'"""
        controls = audio_output.parse_mixer_controls(mixer)
        self.assertEqual([c["name"] for c in controls], ["Speaker", "Headphone", "Auto-Mute Mode"])
        self.assertEqual([c["state"] for c in controls], ["on", "off", "Enabled"])
        self.assertEqual([c["port"] for c in controls], [True, True, False])
        with patch.object(audio_output, "_run_aplay_list", return_value=listing), \
                patch.object(audio_output, "list_mixer_controls", side_effect=[controls, []]) as queried:
            devices = audio_output.list_alsa_devices(pc=True)
        self.assertEqual(queried.call_count, 2)
        self.assertEqual([d["alsa"] for d in devices],
                         ["plughw:CARD=PCH,DEV=0", "plughw:CARD=PCH,DEV=3", "plughw:CARD=DAC,DEV=0"])
        self.assertEqual(devices[0]["label"], "HDA Intel PCH · ALC269VB Analog")
        self.assertEqual(devices[0]["controls"], devices[1]["controls"])

    def test_legacy_intel_pcm_has_no_invented_speaker_port(self):
        listing = "card 1: Intel [HDA Intel], device 0: ALC662 rev1 Analog [ALC662 rev1 Analog]"
        controls = audio_output.parse_mixer_controls(
            "Simple mixer control 'Master',0\n  Capabilities: pvolume pvolume-joined pswitch\n  Mono: Playback 20 [on]\n"
        )
        with patch.object(audio_output, "_run_aplay_list", return_value=listing), \
                patch.object(audio_output, "list_mixer_controls", return_value=controls):
            devices = audio_output.list_alsa_devices(pc=True)
        self.assertEqual(devices[0]["alsa"], "plughw:CARD=Intel,DEV=0")
        self.assertEqual(devices[0]["label"], "HDA Intel · ALC662 rev1 Analog")
        self.assertFalse(devices[0]["controls"][0]["port"])

    def test_no_wifi_and_renamed_adapter(self):
        with tempfile.TemporaryDirectory() as temporary:
            network = Path(temporary)
            (network / "enp1s0").mkdir()
            self.assertEqual(network_wifi.detected_wifi_interface("wlan0", network), "")
            (network / "wlp2s0" / "phy80211").mkdir(parents=True)
            (network / "wlxexternal" / "wireless").mkdir(parents=True)
            self.assertEqual(network_wifi.detected_wifi_interface("wlan0", network), "wlp2s0")
            self.assertEqual(network_wifi.detected_wifi_interface("wlxexternal", network), "wlxexternal")

    def test_boot_export_does_not_copy_journal_lines(self):
        path = Path(__file__).resolve().parents[1] / "image/pc/boot/hardware-report.py"
        spec = importlib.util.spec_from_file_location("hardware_report", path)
        report = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(report)
        journal = ("Failed to open ALSA device /mnt/music/personal file.flac\n"
                   "ACPI BIOS Error (bug): AE_ALREADY_EXISTS\n"
                   "failed to load b43/ucode15.fw (-2)\nssid=private-network\n")
        with patch.object(report, "run", side_effect=lambda *cmd, **kw: journal if cmd[0] == "journalctl" else ""), \
                patch.object(report, "read", return_value=""):
            output = "\n".join(report.collect())
        self.assertIn("audio-output-open-failed: 1", output)
        self.assertIn("acpi-bios-error: 1", output)
        self.assertIn("b43-ucode15-missing: 1", output)
        self.assertNotIn("personal file.flac", output)
        self.assertNotIn("private-network", output)
