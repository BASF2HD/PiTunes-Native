#!/usr/bin/env bash
# Static/rootfs and software-render smoke validation, not a boot/hardware test.
set -euo pipefail
test -x /usr/bin/pitunes-native
test -s /usr/share/xsessions/pitunes-native.desktop
test -s /etc/systemd/user/pitunes-native.service
ldd /usr/bin/pitunes-native >/opt/pitunes/config/native-linked-libs.txt
python3 /opt/pitunes/scripts/audit-native-rootfs.py --output /opt/pitunes/config
test "$(readlink /etc/systemd/system/pitunes-update.service)" = /dev/null
test "$(readlink /etc/systemd/system/pitunes-system-update.service)" = /dev/null
QT_QPA_PLATFORM=offscreen /usr/bin/pitunes-native --demo \
  --screenshot /tmp/pitunes-native-install-check.png --screenshot-delay 50
test -s /tmp/pitunes-native-install-check.png
mv /tmp/pitunes-native-install-check.png /opt/pitunes/config/native-install-check.png
echo 'Native rootfs software-render check passed; hardware boot/audio remains untested.'
