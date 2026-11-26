#!/usr/bin/env bash
# Run inside the target-architecture distro container; source may be read-only.
set -euo pipefail
[[ "$(uname -s)" = Linux ]] || { echo 'Use a target Linux container.' >&2; exit 1; }
source_dir="${1:?Source directory}"
output_dir="${2:?Output directory}"
work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends cmake ninja-build g++ make dpkg-dev ccache \
  qtbase5-dev libqt5svg5-dev ca-certificates
cmake -S "${source_dir}" -B "${work}/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCPACK_DEBIAN_PACKAGE_ARCHITECTURE="$(dpkg --print-architecture)" -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build "${work}/build" --parallel 2
QT_QPA_PLATFORM=offscreen ctest --test-dir "${work}/build" --output-on-failure
(cd "${work}/build" && cpack -G DEB)
mkdir -p "${output_dir}"
cp "${work}"/build/pitunes-native*.deb "${output_dir}/"
dpkg-query -W -f='${binary:Package}\t${Version}\n' >"${output_dir}/build-packages.tsv"
(cd "${output_dir}" && sha256sum ./*.deb >SHA256SUMS)
