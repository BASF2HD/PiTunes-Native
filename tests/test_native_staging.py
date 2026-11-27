"""Exercise actual native staging/patch application for all four targets on a host."""
import importlib.util
import json
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("native_staging", ROOT / "tools/prepare-native-source.py")
staging = importlib.util.module_from_spec(spec)
spec.loader.exec_module(staging)

audit_spec = importlib.util.spec_from_file_location("native_rootfs_audit", ROOT / "tools/audit-native-rootfs.py")
audit = importlib.util.module_from_spec(audit_spec)
audit_spec.loader.exec_module(audit)


class NativeStagingTest(unittest.TestCase):
    def test_ignored_local_environment_file_is_never_imported(self):
        with tempfile.NamedTemporaryFile(prefix=".env.native-staging-", dir=ROOT / "backend", delete=False) as fixture:
            fixture.write(b"DO_NOT_COPY_TEST_FIXTURE=1\n")
        secret = Path(fixture.name)
        self.addCleanup(secret.unlink, missing_ok=True)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "native"
            staging.prepare(output, "amd64")
            self.assertFalse((output / "backend" / secret.name).exists())

    def test_all_targets_have_native_session_and_update_isolation(self):
        with tempfile.TemporaryDirectory() as directory:
            for arch in ("i386", "amd64", "armhf", "arm64"):
                with self.subTest(arch=arch):
                    output = Path(directory) / arch
                    staging.prepare(output, arch)
                    wrapper = (output / "scripts/setup-kiosk.sh").read_text()
                    self.assertIn("setup-native-kiosk.sh", wrapper)
                    setup = (output / "scripts/setup-native-kiosk.sh").read_text()
                    self.assertIn("ln -sfn /dev/null /etc/systemd/system/pitunes-display.service",
                                  (output / "install.sh").read_text())
                    for name in ("pitunes-samba-late.service", "pitunes-startup-scan.service", "pitunes-hotspot.service"):
                        self.assertNotIn("pitunes-display.service", (output / "systemd" / name).read_text())
                    for name in ("pitunes-display.sh", "pitunes-kiosk-launch.sh"):
                        retired = subprocess.run(["bash", str(output / "scripts" / name)], capture_output=True, text=True)
                        self.assertEqual(retired.returncode, 1)
                        self.assertIn("starts through its LightDM session", retired.stderr)
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

    def test_rootfs_audit_rejects_installed_webengine_and_legacy_activation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            files = {
                "var/lib/dpkg/status": "".join(
                    f"Package: {name}\nStatus: install ok installed\nVersion: 1\nArchitecture: i386\n\n"
                    for name in ("pitunes-native", "mpd", "nginx")
                ) + "Package: chromium\nStatus: deinstall ok config-files\nVersion: 1\n",
                "usr/bin/pitunes-native": "",
                "etc/nginx/sites-available/pitunes.conf":
                    "root /opt/pitunes/frontend;\nproxy_pass http://127.0.0.1:8080;\n",
                "opt/pitunes/frontend/index.html": "",
                "opt/pitunes/frontend/assets/app.js": "",
                "opt/pitunes/frontend/assets/styles.css": "",
            }
            for path, content in files.items():
                target = root / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(content)
            units = root / "etc/systemd/system"
            units.mkdir(parents=True)
            (units / "pitunes-display.service").symlink_to("/dev/null")
            report, _ = audit.inspect(root, graphical=False)
            self.assertFalse(report["violations"])

            with (root / "var/lib/dpkg/status").open("a") as manifest:
                manifest.write("\nPackage: qml6-module-qtwebengine\nStatus: install ok installed\nVersion: 1\n")
            (units / "pitunes-samba-late.service").write_text("Wants=pitunes-display.service\n")
            report, _ = audit.inspect(root, graphical=False)
            self.assertEqual(report["forbiddenPackages"], ["qml6-module-qtwebengine"])
            self.assertTrue(report["unexpectedUnitReferences"])
            self.assertEqual(len(report["violations"]), 2)

    def test_refuses_existing_or_source_destination(self):
        with self.assertRaises(ValueError):
            staging.prepare(ROOT / "backend", "amd64")
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ValueError):
                staging.prepare(Path(directory), "arm64")


if __name__ == "__main__":
    unittest.main()
