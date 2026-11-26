#!/usr/bin/env bash
# Static/rootfs and software-render smoke validation, not a boot/hardware test.
set -euo pipefail
test -x /usr/local/bin/pitunes-native
test -s /usr/share/xsessions/pitunes-native.desktop
test -s /etc/systemd/user/pitunes-native.service
for package in chromium chromium-browser chromium-sandbox libqt5webenginewidgets5; do
  if dpkg-query -W -f='${Status}' "${package}" 2>/dev/null | grep -qx 'install ok installed'; then
    echo "Forbidden local-browser package installed: ${package}" >&2; exit 1
  fi
done
test "$(readlink /etc/systemd/system/pitunes-update.service)" = /dev/null
test "$(readlink /etc/systemd/system/pitunes-system-update.service)" = /dev/null
QT_QPA_PLATFORM=offscreen /usr/local/bin/pitunes-native --demo \
  --screenshot /tmp/pitunes-native-install-check.png --screenshot-delay 50
test -s /tmp/pitunes-native-install-check.png
mv /tmp/pitunes-native-install-check.png /opt/pitunes/config/native-install-check.png
dpkg-query -W -f='${binary:Package}\t${Version}\n' >/opt/pitunes/config/native-packages.tsv
echo 'Native rootfs software-render check passed; hardware boot/audio remains untested.'
