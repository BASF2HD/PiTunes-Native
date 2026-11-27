#!/usr/bin/env python3
"""Inspect installed packages and native display wiring in a mounted image root."""
import argparse
import json
import re
from pathlib import Path

BROWSER_COMPONENTS = (
    "chromium", "googlechrome", "firefox", "epiphanybrowser", "midori",
    "electron", "libcef", "nwjs", "qtwebengine", "qt5webengine", "qt6webengine",
    "qtwebview", "qt5webview", "qt6webview", "qtwebkit", "qt5webkit", "qt6webkit",
    "webkitgtk", "webkit2gtk", "webkit41", "webkit60", "wpewebkit",
)


def browser_component(value):
    name = re.sub(r"[^a-z0-9]", "", value.lower())
    return any(item in name for item in BROWSER_COMPONENTS)


def installed_packages(status):
    rows = []
    for paragraph in status.split("\n\n"):
        fields = dict(line.split(": ", 1) for line in paragraph.splitlines() if line and not line[0].isspace() and ": " in line)
        if fields.get("Status", "").split()[-2:] == ["ok", "installed"]:
            rows.append((fields["Package"], fields["Version"], fields.get("Architecture", "")))
    return sorted(rows)


def inspect(root, graphical=True):
    def read(path):
        try:
            return (root / path).read_text(errors="replace")
        except FileNotFoundError:
            return ""

    packages = installed_packages(read("var/lib/dpkg/status"))
    installed = {row[0] for row in packages}
    violations = []

    def require(condition, message):
        if not condition:
            violations.append(message)

    forbidden_packages = [name for name in sorted(installed) if browser_component(name)]
    require(not forbidden_packages, "Installed browser runtime packages: " + ", ".join(forbidden_packages))
    require({"pitunes-native", "mpd", "nginx"} <= installed, "Missing expected native player, MPD, or nginx package")
    require((root / "usr/bin/pitunes-native").is_file(), "Native executable is missing")

    unit_root = root / "etc/systemd/system"
    legacy = unit_root / "pitunes-display.service"
    masked = legacy.is_symlink() and str(legacy.readlink()) == "/dev/null"
    require(masked, "Retired pitunes-display.service can still be loaded")
    unit_references = []
    for unit in sorted(unit_root.rglob("*")):
        if unit.is_symlink() or not unit.is_file() or unit.suffix not in (".service", ".conf"):
            continue
        content = read(unit.relative_to(root))
        for line in content.splitlines():
            line = line.strip()
            if line.startswith(("Wants=", "Requires=", "BindsTo=", "Requisite=")) and "pitunes-display.service" in line:
                unit_references.append(f"{unit.relative_to(root)}: {line}")
            if line.startswith(("ExecStart=", "ExecStartPre=", "ExecStartPost=")):
                command = line.split("=", 1)[1]
                if browser_component(command):
                    unit_references.append(f"{unit.relative_to(root)}: {line}")
    require(not unit_references, "System unit refers to a browser or the retired display")

    desktop = read("usr/share/xsessions/pitunes-native.desktop")
    seat = read("etc/lightdm/lightdm.conf.d/50-pitunes-kiosk.conf")
    session = read("opt/pitunes/scripts/pitunes-native-session.sh")
    native_unit = read("etc/systemd/user/pitunes-native.service")
    if graphical:
        default_target = unit_root / "default.target"
        manager = unit_root / "display-manager.service"
        require(default_target.is_symlink() and default_target.readlink().name == "graphical.target",
                "Graphical target is not the default")
        require(manager.is_symlink() and manager.readlink().name == "lightdm.service", "LightDM does not own display-manager.service")
        require("autologin-session=pitunes-native" in seat.splitlines() and "user-session=pitunes-native" in seat.splitlines(),
                "LightDM autologin does not select the native session")
        require("Exec=/opt/pitunes/scripts/pitunes-native-session.sh" in desktop.splitlines(),
                "Native desktop entry does not select the native session script")
        require("systemctl --user start pitunes-native.service" in session and not browser_component(session),
                "Native session script has unexpected player wiring")
        require("ExecStart=/usr/bin/pitunes-native --fullscreen" in native_unit.splitlines() and not browser_component(native_unit),
                "User service does not select the native executable")

    linked_libs = read("opt/pitunes/config/native-linked-libs.txt")
    if graphical:
        require(bool(linked_libs), "Native executable dependency report is missing")
    if linked_libs:
        require("libQt5Widgets" in linked_libs and not browser_component(linked_libs) and "not found" not in linked_libs,
                "Native ELF dependencies have an unexpected runtime")
    nginx = read("etc/nginx/sites-available/pitunes.conf")
    web_files = ("index.html", "assets/app.js", "assets/styles.css")
    require("root /opt/pitunes/frontend;" in nginx and "proxy_pass http://127.0.0.1:8080;" in nginx,
            "Retained nginx frontend or API proxy is missing")
    require(all((root / "opt/pitunes/frontend" / file).is_file() for file in web_files),
            "Retained browser interface files are missing")
    return {
        "installedPackageCount": len(packages),
        "graphicalImage": graphical,
        "forbiddenPackages": forbidden_packages,
        "legacyDisplayMasked": masked,
        "unexpectedUnitReferences": unit_references,
        "nativeUserExec": next((line[10:] for line in native_unit.splitlines() if line.startswith("ExecStart=")), ""),
        "elfDependenciesCaptured": bool(linked_libs),
        "violations": violations,
    }, packages


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("/"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--prefix", default="native")
    parser.add_argument("--headless", action="store_true")
    parser.add_argument("--report-only", action="store_true",
                        help="Preserve the findings for a historical image without gating its inspection")
    args = parser.parse_args()
    result, packages = inspect(args.root, not args.headless)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / f"{args.prefix}.packages.tsv").write_text(
        "package\tversion\tarchitecture\n" + "".join("\t".join(row) + "\n" for row in packages))
    report = json.dumps(result, indent=2) + "\n"
    (args.output / f"{args.prefix}.rootfs-audit.json").write_text(report)
    print(report, end="")
    if result["violations"] and not args.report_only:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
