#!/usr/bin/env bash
# Reuse original disk builders through a reviewed native-only staging tree.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ARCH="${1:-}"
case "${ARCH}" in i386|amd64|armhf|arm64) ;; *) echo "Usage: sudo $0 i386|amd64|armhf|arm64" >&2; exit 2 ;; esac
[[ "$(uname -s)" = Linux && "$(uname -m)" = x86_64 && "$(id -u)" = 0 ]] || {
  echo 'Image builds require root on x86_64 Linux with loop mounts/binfmt; macOS cannot run this builder.' >&2; exit 1;
}
git_args=(git -c "safe.directory=${ROOT}" -C "${ROOT}")
[[ "$("${git_args[@]}" remote get-url origin)" =~ ^(https://github.com/|git@github.com:)BASF2HD/PiTunes-Native(\.git)?$ ]] || {
  echo 'Native Git remote verification failed.' >&2; exit 1;
}
[[ -z "$("${git_args[@]}" status --porcelain)" ]] || { echo 'Commit source changes before building a source-identified image.' >&2; exit 1; }
COMMIT="$("${git_args[@]}" rev-parse HEAD)"
OUTPUT="${ROOT}/image/out/pitunes-native-${ARCH}.img"
for file in "${OUTPUT}" "${OUTPUT}.xz" "${OUTPUT}.xz.sha256"; do
  [[ ! -e "${file}" && ! -L "${file}" ]] || { echo "Refusing existing output: ${file}" >&2; exit 1; }
done
mkdir -p "${ROOT}/image/work" "${ROOT}/image/out" "${ROOT}/image/cache"
WORK="$(mktemp -d "${ROOT}/image/work/native-${ARCH}.XXXXXX")"
cleanup() {
  # Never recursively remove a tree with a still-mounted chroot after a failed build.
  if python3 - "${WORK}" <<'PY'
import pathlib, sys
prefix = str(pathlib.Path(sys.argv[1]).resolve())
mounts = [line.split()[4].replace('\\040', ' ').replace('\\134', '\\') for line in pathlib.Path('/proc/self/mountinfo').read_text().splitlines()]
raise SystemExit(0 if any(m == prefix or m.startswith(prefix + '/') for m in mounts) else 1)
PY
  then echo "Preserved mounted build tree for recovery: ${WORK}" >&2
  else rm -rf "${WORK}"
  fi
}
trap cleanup EXIT
python3 "${ROOT}/tools/prepare-native-source.py" --arch "${ARCH}" --output "${WORK}/source"
if [[ "${ARCH}" = i386 || "${ARCH}" = amd64 ]]; then
  PITUNES_INSTALL_COMMIT="${COMMIT}" bash "${WORK}/source/tools/build-pc-image.sh" --arch "${ARCH}" --output "${OUTPUT}"
else
  # Compressed base hashes were read from the official Raspberry Pi download server.
  source "${ROOT}/image/build.env"
  if [[ "${ARCH}" = armhf ]]; then BASE_URL="${RPIOS_LITE_ARMHF_URL}"; else BASE_URL="${RPIOS_LITE_ARM64_URL}"; fi
  BASE_XZ="${ROOT}/image/cache/$(basename "${BASE_URL}")"
  EXPECTED="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["targets"][sys.argv[2]]["baseSha256"])' "${ROOT}/image/native-targets.json" "${ARCH}")"
  if [[ ! -f "${BASE_XZ}" ]]; then
    curl --fail --location --retry 3 "${BASE_URL}" -o "${BASE_XZ}.partial"
    mv "${BASE_XZ}.partial" "${BASE_XZ}"
  fi
  printf '%s  %s\n' "${EXPECTED}" "${BASE_XZ}" | sha256sum --check -
  xz -dc "${BASE_XZ}" >"${WORK}/base.img"
  PITUNES_INSTALL_COMMIT="${COMMIT}" bash "${WORK}/source/tools/build-flashable-image.sh" \
    --arch "${ARCH}" --base-image "${WORK}/base.img" --output "${OUTPUT}" --kiosk
fi
# Both underlying builders already verify compression integrity.
(cd "${ROOT}/image/out" && sha256sum "$(basename "${OUTPUT}").xz" >"$(basename "${OUTPUT}").xz.sha256")
printf 'PiTunes Native development image\nArchitecture: %s\nSource: %s\nBoot/hardware validation: NOT YET TESTED\n' \
  "${ARCH}" "${COMMIT}" >"${OUTPUT}.build.txt"
