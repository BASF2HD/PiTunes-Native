#!/usr/bin/env bash
# Legacy web deployment. Native requires a target-matched package.
set -euo pipefail

echo "Native publication/deployment/OTA requires independent validation; this legacy entry point is disabled." >&2
exit 1
