#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-auto}"
HAT_OVERLAY="${HAT_OVERLAY:-}"
MIXER_TYPE="${MIXER_TYPE:-software}"
ALSA_DEVICE="${ALSA_DEVICE:-default}"
MIXER_CONTROL="${MIXER_CONTROL:-}"

if [ "$(id -u)" -ne 0 ]; then
  echo "Run as root: sudo ./configure-mpd.sh [auto|usb-dac|dac-hat|hdmi|headphones]"
  exit 1
fi

case "${MIXER_TYPE}" in
  software|hardware|none) ;;
  *) echo "Invalid ALSA mixer type." >&2; exit 2 ;;
esac
if [ "${MODE}" = "pc-alsa" ]; then
  if [[ ! "${ALSA_DEVICE}" =~ ^(default|(plughw|hw):([0-9]+,[0-9]+|CARD=[A-Za-z0-9_]+,DEV=[0-9]+))$ ]]; then
    echo "Select a valid ALSA playback device." >&2
    exit 2
  fi
  if [ -n "${MIXER_CONTROL}" ]; then
    control_pattern='^[ A-Za-z0-9_.+()-]{1,60}$'
    if [[ "${MIXER_TYPE}" != hardware || ! "${ALSA_DEVICE}" =~ CARD=([A-Za-z0-9_]+), || ! "${MIXER_CONTROL}" =~ ${control_pattern} ]]; then
      echo "Invalid hardware mixer control." >&2
      exit 2
    fi
    card="${ALSA_DEVICE#*CARD=}"
    card="${card%%,*}"
  fi
fi

if ! mountpoint -q /mnt/music; then
  install -d -m 0775 -o mpd -g audio /mnt/music
fi
install -d -m 0755 /var/lib/mpd/playlists
install -d -m 0755 /var/log/mpd
touch /var/log/mpd/mpd.log
chown -R mpd:audio /var/lib/mpd /var/log/mpd
if [ -w /mnt/music ] && ! mountpoint -q /mnt/music; then
  chown -R mpd:audio /mnt/music
fi

config="$(mktemp /etc/.pitunes-mpd.XXXXXX)"
previous=""
trap 'rm -f -- "${config}" "${previous}"' EXIT
cat >"${config}" <<'EOF'
music_directory         "/mnt/music"
playlist_directory      "/var/lib/mpd/playlists"
db_file                 "/var/lib/mpd/tag_cache"
log_file                "/var/log/mpd/mpd.log"
pid_file                "/run/mpd/pid"
state_file              "/var/lib/mpd/state"
sticker_file            "/var/lib/mpd/sticker.sql"

user                    "mpd"
bind_to_address         "127.0.0.1"
port                    "6600"
auto_update             "no"
restore_paused          "yes"
filesystem_charset      "UTF-8"
metadata_to_use         "artist,album,title,track,name,genre,date,albumartist,disc"
follow_outside_symlinks "yes"
follow_inside_symlinks  "yes"
zeroconf_enabled        "yes"
zeroconf_name           "PiTunes MPD"

audio_output {
        type            "alsa"
        name            "ALSA default"
        mixer_type      "${MIXER_TYPE}"
}

audio_buffer_size       "2048"
buffer_before_play      "10%"
max_playlist_length     "16384"
max_connections         "20"
EOF

sed -i 's/${MIXER_TYPE}/'"${MIXER_TYPE}"'/' "${config}"
case "${MODE}" in
  pc-alsa)
    sed -i "/name .*ALSA default/a\\        device          \"${ALSA_DEVICE}\"" "${config}"
    if [ "${ALSA_DEVICE}" = default ]; then
      sed -i '/device *"default"/i\        # PiTunes automatic playback device' "${config}"
    fi
    if [ -n "${MIXER_CONTROL}" ]; then
      sed -i "/mixer_type/a\\        mixer_device    \"hw:CARD=${card}\"\n        mixer_control   \"${MIXER_CONTROL}\"" "${config}"
    fi
    ;;
  auto)
    rm -f /etc/asound.conf
    ;;
  usb-dac)
    cat >/etc/asound.conf <<'EOF'
defaults.pcm.card 1
defaults.ctl.card 1
EOF
    ;;
  dac-hat)
    cat >/etc/asound.conf <<'EOF'
defaults.pcm.card 0
defaults.ctl.card 0
EOF
    if [ -n "${HAT_OVERLAY}" ]; then
      BOOT_CONFIG="/boot/firmware/config.txt"
      [ -f /boot/config.txt ] && BOOT_CONFIG="/boot/config.txt"
      if ! grep -q "^dtoverlay=${HAT_OVERLAY}$" "${BOOT_CONFIG}"; then
        printf '\n# PiTunes DAC HAT\ndtoverlay=%s\n' "${HAT_OVERLAY}" >>"${BOOT_CONFIG}"
      fi
    fi
    ;;
  hdmi)
    rm -f /etc/asound.conf
    if command -v raspi-config >/dev/null 2>&1; then
      raspi-config nonint do_audio 2 || true
    fi
    ;;
  headphones)
    rm -f /etc/asound.conf
    if command -v raspi-config >/dev/null 2>&1; then
      raspi-config nonint do_audio 1 || true
    fi
    ;;
  *)
    echo "Unknown audio mode: ${MODE}"
    exit 1
    ;;
esac

if [[ "${MODE}" = pc-alsa && "${PITUNES_IMAGE_BUILD:-0}" != 1 ]] && [ -f /etc/mpd.conf ]; then
  previous="$(mktemp /etc/.pitunes-mpd-previous.XXXXXX)"
  cp -p /etc/mpd.conf "${previous}"
fi
chmod 0644 "${config}"
mv -f -- "${config}" /etc/mpd.conf
if [[ "${MODE}" = pc-alsa && "${PITUNES_IMAGE_BUILD:-0}" != 1 ]]; then
  if ! timeout 12s systemctl restart mpd; then
    if [ -n "${previous}" ]; then
      mv -f -- "${previous}" /etc/mpd.conf
      timeout 8s systemctl restart mpd || true
    fi
    echo "MPD could not start with this ALSA configuration. See the MPD service journal." >&2
    exit 1
  fi
else
  timeout 12s systemctl restart mpd 2>/dev/null || true
fi
echo "MPD configured for ${MODE} output and /mnt/music library."
