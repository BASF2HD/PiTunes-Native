#!/usr/bin/env python3
"""Stage reviewed reusable source and native overlays; never edit original PiTunes.

Keeps image partition/firmware/recovery code, replaces only browser packaging and
launch ownership. Requires an absent destination and refuses non-native identity.
"""
import argparse
import json
import re
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ITEMS = ("backend", "frontend", "config", "nginx", "scripts", "systemd", "tools", "image",
         "native", "CMakeLists.txt", "install.sh", "configure-mpd.sh", "LICENSE", "THIRD_PARTY_NOTICES.md")


def replace_once(path, old, new):
    text = path.read_text()
    if text.count(old) != 1:
        raise RuntimeError(f"Source contract changed in {path.name}; expected one replacement")
    path.write_text(text.replace(old, new, 1))


def prepare(destination: Path, arch: str):
    if arch not in ("i386", "amd64", "armhf", "arm64"):
        raise ValueError("Unknown target architecture")
    if json.loads((ROOT / "config/version.json").read_text()).get("product") != "PiTunes-Native":
        raise RuntimeError("Refusing a source tree without native product identity")
    destination = destination.resolve()
    if destination.exists() or (destination.is_relative_to(ROOT) and not destination.is_relative_to(ROOT / "image/work")):
        raise ValueError("Destination must be absent; inside this checkout use image/work only")
    destination.mkdir(parents=True)
    for item in ITEMS:
        source = ROOT / item
        if source.is_dir():
            shutil.copytree(source, destination / item,
                            ignore=shutil.ignore_patterns("__pycache__", ".DS_Store", "out", "work", "cache"))
        else:
            shutil.copy2(source, destination / item)
    (destination / "config/.native-stage").write_text(arch + "\n")
    if arch in ("i386", "amd64"):
        # Preserve proven PC ALSA/mount/recovery adaptations. Browser-specific hunks
        # are intentionally excluded, rather than installing then removing Chromium.
        patch = (ROOT / "image/pc/runtime.patch").read_text()
        sections = re.split(r"(?=^diff --git )", patch, flags=re.MULTILINE)
        patch = "".join(s for s in sections if not s.startswith((
            "diff --git a/scripts/setup-kiosk.sh ", "diff --git a/scripts/pitunes-kiosk-launch.sh ")))
        subprocess.run(["patch", "--batch", "--forward", "--fuzz=0", "-d", str(destination), "-p1"],
                       input=patch, text=True, check=True, capture_output=True)
        builder = destination / "tools/build-pc-image.sh"
        replace_once(builder,
                     'for patch_file in runtime.patch render-on-demand.patch kiosk-readiness.patch; do\n'
                     '  patch --batch --forward --fuzz=0 -d "${SOURCE}" -p1 <"${ROOT}/image/pc/${patch_file}"\ndone',
                     '# Native preparation already applied reviewed non-browser PC adaptations.')
        replace_once(builder,
                     '{backend,config,frontend,nginx,scripts,systemd,install.sh,configure-mpd.sh}',
                     '{backend,config,frontend,nginx,scripts,systemd,native,CMakeLists.txt,install.sh,configure-mpd.sh}')
        replace_once(builder, '${ROOT}/image/out/pitunes-${ARCH}.img',
                     '${ROOT}/image/out/pitunes-native-${ARCH}.img')
    # These compatibility entry points now delegate only to the native session.
    shutil.copy2(ROOT / "scripts/setup-native-kiosk.sh", destination / "scripts/setup-kiosk.sh")
    for name in ("pitunes-display.sh", "pitunes-kiosk-launch.sh"):
        (destination / "scripts" / name).write_text(
            '#!/usr/bin/env bash\nset -euo pipefail\nexec /opt/pitunes/scripts/pitunes-native-session.sh\n')
    installer = destination / "install.sh"
    replace_once(installer, 'echo "Creating service user and directories..."',
                 '"${SCRIPT_DIR}/scripts/install-native-app.sh" "${SCRIPT_DIR}"\n\n'
                 'echo "Creating service user and directories..."')
    replace_once(installer, 'cp -a "${SCRIPT_DIR}/scripts" "${INSTALL_DIR}/"',
                 'cp -a "${SCRIPT_DIR}/scripts" "${INSTALL_DIR}/"\n'
                 'cp -a "${SCRIPT_DIR}/systemd" "${INSTALL_DIR}/"')
    installer.write_text(installer.read_text() + '\n'
        'rm -f /etc/systemd/system/pitunes-update.service /etc/systemd/system/pitunes-system-update.service '
        '/etc/systemd/system/pitunes-system-boot-check.service\n'
        'systemctl mask pitunes-update.service pitunes-system-update.service pitunes-system-boot-check.service\n')
    # Fix the inherited literal mixer token in staged Pi MPD output; PC has its own validated branch.
    if arch in ("armhf", "arm64"):
        replace_once(destination / "configure-mpd.sh", 'case "${MODE}" in',
                     'case "${MIXER_TYPE}" in software|hardware|none) ;; *) exit 2 ;; esac\n'
                     'sed -i "s/\\${MIXER_TYPE}/${MIXER_TYPE}/g" /etc/mpd.conf\n\ncase "${MODE}" in')
    # Do not ship the original shared recovery-network credential. First boot
    # creates a device-specific secret in /etc; template contains no credential.
    replace_once(destination / "config/wifi-hotspot.conf", "AP_PASSWORD=pitunesaudio", "AP_PASSWORD=")
    replace_once(destination / "scripts/wifi-hotspot.sh", '${AP_PASSWORD:-pitunesaudio}', '${AP_PASSWORD:?Recovery hotspot password has not been provisioned}')
    firstboot = destination / "scripts/firstboot.sh"
    replace_once(firstboot, 'touch "${DONE_FILE}"',
                 'if ! grep -qE "^AP_PASSWORD=.+" /etc/pitunes/wifi-hotspot.conf; then\n'
                 '  secret="$(od -An -N12 -tx1 /dev/urandom | tr -d \'[:space:]\')"\n'
                 '  printf "\\nAP_PASSWORD=%s\\n" "${secret}" >>/etc/pitunes/wifi-hotspot.conf\n'
                 '  chmod 0640 /etc/pitunes/wifi-hotspot.conf\nfi\ntouch "${DONE_FILE}"')
    (destination / "image/build.env").write_text((destination / "image/build.env").read_text().replace('IMAGE_BASENAME="pitunes"', 'IMAGE_BASENAME="pitunes-native"'))
    pi_builder = destination / "tools/build-flashable-image.sh"
    replace_once(pi_builder, '"product": "PiTunes",', '"product": "PiTunes-Native",')
    replace_once(pi_builder, 'xz -T0 -9 -k -f', 'xz -T2 -3 -k -f')
    # Never copy host build directories or caches into the Pi disk image.
    replace_once(destination / "tools/build-flashable-image.sh", "--exclude '.git' \\",
                 "--exclude '.git' \\\n  --exclude 'build' \\\n  --exclude '.cache' \\\n  --exclude '.venv' \\")
    for path in destination.rglob("*.sh"):
        path.chmod(path.stat().st_mode | 0o111)
        subprocess.run(["bash", "-n", str(path)], check=True)
    return destination


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arch", choices=("i386", "amd64", "armhf", "arm64"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(prepare(args.output, args.arch))
