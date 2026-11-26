"""Exercise actual native staging/patch application for all four targets on a host."""
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("native_staging", ROOT / "tools/prepare-native-source.py")
staging = importlib.util.module_from_spec(spec)
spec.loader.exec_module(staging)


class NativeStagingTest(unittest.TestCase):
    def test_all_targets_have_native_session_and_update_isolation(self):
        with tempfile.TemporaryDirectory() as directory:
            for arch in ("i386", "amd64", "armhf", "arm64"):
                with self.subTest(arch=arch):
                    output = Path(directory) / arch
                    staging.prepare(output, arch)
                    setup = (output / "scripts/setup-kiosk.sh").read_text()
                    self.assertNotIn("chromium", setup.lower())
                    self.assertIn("pitunes-native.desktop", setup)
                    self.assertIn("install-native-app.sh", (output / "install.sh").read_text())
                    self.assertIn("systemctl mask pitunes-update", (output / "install.sh").read_text())
                    self.assertIn("Native OTA is disabled", (output / "scripts/pitunes-update.sh").read_text())
                    self.assertNotIn("AP_PASSWORD=pitunesaudio", (output / "config/wifi-hotspot.conf").read_text())
                    self.assertIn("/dev/urandom", (output / "scripts/firstboot.sh").read_text())
                    self.assertEqual(json.loads((output / "config/version.json").read_text())["product"], "PiTunes-Native")
                    if arch in ("i386", "amd64"):
                        self.assertIn('pc-alsa)', (output / "configure-mpd.sh").read_text())
                        builder = (output / "tools/build-pc-image.sh").read_text()
                        self.assertNotIn('for patch_file in', builder)
                        self.assertIn('native,CMakeLists.txt', builder)
                        self.assertIn('search.fs_uuid', (output / "image/pc/boot/configure.sh").read_text())
                    else:
                        self.assertIn('s/\\${MIXER_TYPE}/${MIXER_TYPE}/g', (output / "configure-mpd.sh").read_text())
                        self.assertIn('raspios-bookworm', (output / "image/build.env").read_text())
                    self.assertFalse((output / ".git").exists())
                    self.assertFalse((output / "image/work").exists())

    def test_refuses_existing_or_source_destination(self):
        with self.assertRaises(ValueError):
            staging.prepare(ROOT / "backend", "amd64")
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ValueError):
                staging.prepare(Path(directory), "arm64")


if __name__ == "__main__":
    unittest.main()
