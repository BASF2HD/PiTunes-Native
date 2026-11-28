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
                patch.object(audio_output, "_proc_pcm_devices", return_value=[]), \
                patch.object(audio_output, "list_mixer_controls", side_effect=[controls, []]) as queried:
            devices = audio_output.list_alsa_devices(pc=True)
        self.assertEqual(queried.call_count, 2)
        self.assertEqual([d["alsa"] for d in devices],
                         ["plughw:CARD=PCH,DEV=0", "plughw:CARD=PCH,DEV=3", "plughw:CARD=DAC,DEV=0"])
        self.assertEqual(devices[0]["label"], "Built-in Audio (ALC269VB Analog)")
        self.assertEqual(devices[0]["controls"], devices[1]["controls"])

    def test_legacy_intel_pcm_has_no_invented_speaker_port(self):
        listing = "card 1: Intel [HDA Intel], device 0: ALC662 rev1 Analog [ALC662 rev1 Analog]"
        controls = audio_output.parse_mixer_controls(
            "Simple mixer control 'Master',0\n  Capabilities: pvolume pvolume-joined pswitch\n  Mono: Playback 20 [on]\n"
        )
        with patch.object(audio_output, "_run_aplay_list", return_value=listing), \
                patch.object(audio_output, "_proc_pcm_devices", return_value=[]), \
                patch.object(audio_output, "list_mixer_controls", return_value=controls):
            devices = audio_output.list_alsa_devices(pc=True)
        self.assertEqual(devices[0]["alsa"], "plughw:CARD=Intel,DEV=0")
        self.assertEqual(devices[0]["label"], "Built-in Audio (ALC662 rev1 Analog)")
        self.assertFalse(devices[0]["controls"][0]["port"])

    def test_alc272_ports_and_muting_are_detected(self):
        listing = "card 0: Intel [HDA Intel], device 0: ALC272 Analog [ALC272 Analog]"
        with patch.object(audio_output, "_run_aplay_list", return_value=listing), \
                patch.object(audio_output, "_proc_pcm_devices", return_value=[]), \
                patch.object(audio_output, "_analogue_codec", return_value="Realtek ALC272"), \
                patch.object(audio_output, "list_mixer_controls", return_value=[]):
            outputs = audio_output.list_alsa_devices(pc=True)
        self.assertEqual(outputs[0]["label"], "Built-in Audio (Realtek ALC272)")
        self.assertEqual(outputs[0]["alsa"], "plughw:CARD=Intel,DEV=0")
        outputs[0]["controls"] = [{"name": "Speaker", "port": True, "volume": True, "state": "on"},
                                  {"name": "Auto-Mute Mode", "port": False, "volume": False, "state": "Enabled"}]
        with patch.object(audio_output, "_selected_pc_device", return_value=outputs[0]), \
                patch.object(audio_output, "_run_pc_audio") as run:
            audio_output.apply_pc_mixer({"alsa": outputs[0]["alsa"], "control": "Speaker", "state": "mute"})
            run.assert_called_once_with(["amixer", "-q", "-c", "0", "sset", "Speaker", "mute"])
            with self.assertRaises(ValueError):
                audio_output.apply_pc_mixer({"alsa": outputs[0]["alsa"], "control": "Speaker", "percent": 101})

    def test_automatic_route_prefers_local_analogue(self):
        import sys
        with patch.dict(sys.modules, {"audio_output": audio_output}):
            from backend import pc_audio_boot
        self.assertEqual(pc_audio_boot.preferred_pcm([
            {"device_desc": "USB Audio", "card_id": "USB", "short_name": "USB", "card": 0, "device": 0},
            {"device_desc": "ALC272 Analog", "card_id": "HDA Intel", "short_name": "Intel", "card": 1, "device": 0},
        ]), "plughw:CARD=Intel,DEV=0")

    def test_no_wifi_and_renamed_adapter(self):
        with tempfile.TemporaryDirectory() as temporary:
            network = Path(temporary)
            (network / "enp1s0").mkdir()
            self.assertEqual(network_wifi.detected_wifi_interface("wlan0", network), "")
            (network / "wlp2s0" / "phy80211").mkdir(parents=True)
            (network / "wlxexternal" / "wireless").mkdir(parents=True)
            self.assertEqual(network_wifi.detected_wifi_interface("wlan0", network), "wlp2s0")
            self.assertEqual(network_wifi.detected_wifi_interface("wlxexternal", network), "wlxexternal")

    def test_kernel_pcm_fallback_only_lists_playback_endpoints(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            (root / "pcm").write_text("00-00: ALC268 Analog : ALC268 Analog : playback 1 : capture 1\n"
                                      "00-02: Input : Input : capture 1\n")
            (root / "cards").write_text(" 0 [Intel ]: HDA-Intel - HDA Intel\n")
            (root / "card0").mkdir()
            (root / "card0" / "id").write_text("Intel\n")
            with patch.object(audio_output, "_run_aplay_list", return_value=""), \
                    patch.object(audio_output, "_proc_pcm_devices", return_value=audio_output._proc_pcm_devices(root)), \
                    patch.object(audio_output, "list_mixer_controls", return_value=[]):
                outputs = audio_output.list_alsa_devices(pc=True)
        self.assertEqual([(d["alsa"], d["label"]) for d in outputs],
                         [("plughw:CARD=Intel,DEV=0", "Built-in Audio (ALC268 Analog)")])

    def test_wireless_pci_id_survives_failed_driver_binding(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            card = root / "0000:05:00.0"
            card.mkdir()
            for key, value in {"class": "0x028000", "vendor": "0x14e4", "device": "0x4315"}.items():
                (card / key).write_text(value)
            self.assertEqual(network_wifi._pci_wifi_devices(root), [{"id": "14e4:4315", "driver": ""}])

    def test_b43_import_keeps_existing_firmware_and_rejects_symlinks(self):
        report = self.hardware_report()
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            source = root / "INSTALL-B43"
            source.mkdir()
            (source / "ucode15.fw").write_bytes(b"fixture")
            (source / "b0g0initvals5.fw").symlink_to(source / "ucode15.fw")
            destination = root / "b43"
            with self.assertRaisesRegex(RuntimeError, "invalid firmware file"):
                report.install_b43(source, destination)
            self.assertFalse(destination.exists())
            (source / "b0g0initvals5.fw").unlink()
            (source / "b0g0initvals5.fw").write_bytes(b"fixture")
            self.assertIn("installed", report.install_b43(source, destination))
            (source / "ucode15.fw").write_bytes(b"replacement")
            self.assertIn("already installed", report.install_b43(source, destination))
            self.assertEqual((destination / "ucode15.fw").read_bytes(), b"fixture")

    @staticmethod
    def hardware_report():
        path = Path(__file__).resolve().parents[1] / "image/pc/boot/hardware-report.py"
        spec = importlib.util.spec_from_file_location("hardware_report", path)
        report = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(report)
        return report

    def test_boot_export_does_not_copy_journal_lines(self):
        report = self.hardware_report()
        journal = ("Failed to open ALSA device /mnt/music/personal file.flac\n"
                   "ACPI BIOS Error (bug): AE_ALREADY_EXISTS\n"
                   "failed to load b43/ucode15.fw (-2)\nssid=private-network\n")
        def file_data(name, *args, **kwargs):
            return {
                "cpuinfo": "model name : Intel(R) Atom(TM) CPU N270 @ 1.60GHz",
                "product_name": "Standard PC (Q35 + ICH9, 2009)",
            }.get(Path(name).name, "")

        with patch.object(report, "run", side_effect=lambda *cmd, **kw: journal if cmd[0] == "journalctl" else ""), \
                patch.object(report, "read", side_effect=file_data):
            output = "\n".join(report.collect())
        self.assertIn("audio-output-open-failed: 1", output)
        self.assertIn("acpi-bios-error: 1", output)
        self.assertIn("b43-ucode15-missing: 1", output)
        self.assertIn("CPU: Intel(R) Atom(TM) CPU N270 @ 1.60GHz", output)
        self.assertIn("product_name: Standard PC (Q35 + ICH9, 2009)", output)
        self.assertNotIn("personal file.flac", output)
        self.assertNotIn("private-network", output)
