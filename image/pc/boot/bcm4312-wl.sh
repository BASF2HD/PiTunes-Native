#!/usr/bin/env bash
# Bind an owner-installed Bookworm wl module only to the known BCM4312 LP-PHY.
set -euo pipefail
if ! [[ -f /etc/pitunes-pc-image && "$(cat /etc/pitunes-pc-image)" = i386 ]]; then
  exit 0
fi
module="$(modinfo -k "$(uname -r)" -n wl 2>/dev/null)" || exit 0
[[ -f "${module}" ]] || exit 0
for device in /sys/bus/pci/devices/*; do
  [[ -r "${device}/vendor" && -r "${device}/device" ]] || continue
  [[ "$(<"${device}/vendor")" = 0x14e4 && "$(<"${device}/device")" = 0x4315 ]] || continue
  current="$(readlink "${device}/driver" || true)"
  current="${current##*/}"
  case "${current}" in
    wl) continue ;;
    ''|b43-pci-bridge|ssb) ;;
    *) continue ;;
  esac

  [[ -w "${device}/driver_override" ]] || continue
  modprobe wl
  [[ -w /sys/bus/pci/drivers/wl/bind ]] || continue
  address="${device##*/}"
  printf 'wl\n' >"${device}/driver_override"
  if [[ -n "${current}" && -w "${device}/driver/unbind" ]]; then
    if ! printf '%s\n' "${address}" >"${device}/driver/unbind"; then
      printf '\n' >"${device}/driver_override"
      exit 1
    fi
  fi
  if [[ "$(readlink "${device}/driver" || true)" != */wl ]] &&
      ! printf '%s\n' "${address}" >/sys/bus/pci/drivers/wl/bind; then
    printf '\n' >"${device}/driver_override"
    if [[ -n "${current}" ]]; then
      printf '%s\n' "${address}" >"/sys/bus/pci/drivers/${current}/bind" || true
    fi
    echo "BCM4312 could not bind to wl; restored the original driver preference." >&2
    exit 1
  fi
  echo "BCM4312 has selected wl."
done
