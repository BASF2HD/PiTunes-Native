#!/usr/bin/env bash
# Install a minimal X11 session, never a browser or desktop environment.
set -euo pipefail
[[ "$(uname -s)" = Linux && "$(id -u)" = 0 ]] || { echo 'Run on the Linux image target as root.' >&2; exit 1; }
KIOSK_USER="${KIOSK_USER:-pi}"
id "${KIOSK_USER}" >/dev/null
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
  xserver-xorg x11-xserver-utils xinit openbox lightdm lightdm-gtk-greeter \
  dbus-user-session fonts-dejavu-core fonts-noto-core
for group in video input render; do
  getent group "${group}" >/dev/null && usermod -aG "${group}" "${KIOSK_USER}"
done
install -d /usr/share/xsessions /etc/lightdm/lightdm.conf.d /etc/systemd/user
user_home="$(getent passwd "${KIOSK_USER}" | cut -d: -f6)"
install -d -m 0700 -o "${KIOSK_USER}" -g "$(id -gn "${KIOSK_USER}")" "${user_home}/.config" "${user_home}/.cache"
cat >/usr/share/xsessions/pitunes-native.desktop <<'EOF'
[Desktop Entry]
Name=PiTunes Native
Type=Application
Exec=/opt/pitunes/scripts/pitunes-native-session.sh
EOF
cat >/etc/lightdm/lightdm.conf.d/50-pitunes-kiosk.conf <<EOF
[Seat:*]
autologin-user=${KIOSK_USER}
autologin-user-timeout=0
autologin-session=pitunes-native
user-session=pitunes-native
greeter-hide-users=true
xserver-command=X -s 0 -dpms
EOF
install -m 0644 /opt/pitunes/systemd/pitunes-native.service /etc/systemd/user/pitunes-native.service
test "$(readlink /etc/systemd/system/pitunes-display.service)" = /dev/null
systemctl enable lightdm
systemctl set-default graphical.target
"/opt/pitunes/scripts/verify-native-install.sh"
echo 'Native session installed; MPD and API remain independent.'
