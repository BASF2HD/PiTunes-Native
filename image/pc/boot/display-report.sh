#!/usr/bin/env bash
set -euo pipefail
if mountpoint -q /boot/efi; then
  /usr/local/libexec/pitunes-pc/hardware-report --display-failure
else
  echo 'PiTunes display failed; the USB boot partition is unavailable for diagnostics.'
fi
echo 'PiTunes display failed. Read pitunes-display.log on the PITUNES_EFI USB partition.'
