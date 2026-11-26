#!/usr/bin/env bash
# Build in the architecture's own image root; never install a Mac binary into Linux.
set -euo pipefail
[[ "$(uname -s)" = Linux && "$(id -u)" = 0 && "${PITUNES_IMAGE_BUILD:-0}" = 1 ]] || {
  echo 'Native image application installation requires a Linux image chroot.' >&2; exit 1;
}
source_dir="${1:?Source directory required}"
work="$(mktemp -d /var/tmp/pitunes-native-build.XXXXXX)"
trap 'rm -rf "${work}"' EXIT
build_packages=(cmake ninja-build g++ make dpkg-dev file qtbase5-dev libqt5svg5-dev)
new_packages=()
for package in "${build_packages[@]}"; do
  if ! dpkg-query -W -f='${Status}' "${package}" 2>/dev/null | grep -qx 'install ok installed'; then
    new_packages+=("${package}")
  fi
done
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${build_packages[@]}"
cmake -S "${source_dir}" -B "${work}/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DCPACK_DEBIAN_PACKAGE_ARCHITECTURE="$(dpkg --print-architecture)"
cmake --build "${work}/build" --parallel "${PITUNES_BUILD_JOBS:-2}"
(cd "${work}/build" && cpack -G DEB)
# apt resolves the exact distro's shared-library dependency names (including t64).
DEBIAN_FRONTEND=noninteractive apt-get install -y "${work}"/build/pitunes-native*.deb
if [[ "${#new_packages[@]}" -gt 0 ]]; then
  DEBIAN_FRONTEND=noninteractive apt-get purge -y "${new_packages[@]}"
  DEBIAN_FRONTEND=noninteractive apt-get autoremove --purge -y
fi
if [[ ! -x /usr/bin/pitunes-native ]]; then
  dpkg -L pitunes-native >&2
  echo 'Native package did not install the expected /usr/bin executable.' >&2; exit 1
fi
if ldd /usr/bin/pitunes-native | grep -q 'not found'; then
  echo 'Native runtime has missing libraries.' >&2; exit 1
fi
install -d /opt/pitunes/config
dpkg-query -W -f='${binary:Package}\t${Version}\n' >/opt/pitunes/config/native-packages.tsv
