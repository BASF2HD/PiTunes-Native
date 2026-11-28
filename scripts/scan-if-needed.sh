#!/usr/bin/env bash
set -euo pipefail

DB="/var/lib/mpd/tag_cache"

# MPD can create an empty database before the USB music source mounts.
# Update a mounted source even when the database file already exists.
if mountpoint -q /mnt/music || [ ! -s "${DB}" ]; then
  mpc -h 127.0.0.1 update >/dev/null 2>&1 || true
fi
