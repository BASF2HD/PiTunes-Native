#!/usr/bin/env bash
# Shared by the station and recovery-hotspot helpers.
find_wifi_interface() {
  local preferred="${1:-}" path
  if [[ -n "${preferred}" && "${preferred}" != */* ]] &&
      [[ -d "/sys/class/net/${preferred}/phy80211" || -d "/sys/class/net/${preferred}/wireless" ]]; then
    printf '%s\n' "${preferred}"
    return 0
  fi
  local -a devices=(/sys/class/net/*)
  for path in "${devices[@]}"; do
    if [[ -d "${path}/phy80211" || -d "${path}/wireless" ]]; then
      printf '%s\n' "${path##*/}"
      return 0
    fi
  done
  return 1
}
