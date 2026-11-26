#!/usr/bin/env bash
# Native-only compatibility entry point.
set -euo pipefail
exec "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/pitunes-native-session.sh" "$@"
