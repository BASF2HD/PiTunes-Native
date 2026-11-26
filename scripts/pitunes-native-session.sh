#!/usr/bin/env bash
# LightDM owns X/PAM, Openbox supplies only window management, systemd owns GUI recovery.
set -euo pipefail
export QT_QPA_PLATFORM=xcb
xsetroot -solid '#000000' 2>/dev/null || true
xset s off 2>/dev/null || true
xset s 0 0 2>/dev/null || true
xset dpms 0 0 0 2>/dev/null || true
xset -dpms 2>/dev/null || true
xset s noblank 2>/dev/null || true
systemctl --user import-environment DISPLAY XAUTHORITY
dbus-update-activation-environment --systemd DISPLAY XAUTHORITY
openbox &
wm_pid=$!
trap 'systemctl --user stop pitunes-native.service; kill "${wm_pid}" 2>/dev/null || true' EXIT
systemctl --user start pitunes-native.service
wait "${wm_pid}"
