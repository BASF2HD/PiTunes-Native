# Native packages and disk images

## Application builds

macOS development uses the CMake Debug/Release commands in DEVELOPMENT.md.
Linux uses Qt 5.15 Widgets/Network/Concurrent/SVG, C++17, CMake >=3.16; presets
require a newer CMake supporting preset schema 3. No `-march=native` is used.

The **Native application and backend** GitHub-hosted workflow tests the backend
and compiles/tests/packages i386, amd64, armhf and arm64 separately. Container
base manifests are pinned by verified Docker registry digest in
`image/native-targets.json`. Debian packages are installed from the respective
security-supported suite at build time and recorded in `build-packages.tsv`.
This is not yet a byte-for-byte reproducible snapshot of the apt repository.

ARM containers validate Debian Bookworm ABI and compiled application contracts
under QEMU user emulation. They are not Raspberry Pi OS images and do not validate
Pi firmware, HATs or display/input. The Pi image builder compiles in the actual
Pi OS rootfs. Package/runtime arch must match; never install a Mac binary there.

## Image build

Use only `tools/build-native-image.sh`, on root x86_64 Linux with loop mounts,
debootstrap, qemu-user-static/binfmt, filesystem/GRUB and packaging tools. On a Mac,
use the GitHub-hosted **Native development disk image** manual workflow; no
manually maintained extra development machine is required.

```sh
sudo bash tools/build-native-image.sh i386
sudo bash tools/build-native-image.sh amd64
sudo bash tools/build-native-image.sh armhf
sudo bash tools/build-native-image.sh arm64
```

Run one target at a time. Refuses a wrong remote, dirty source, non-Linux host,
existing output or raw disk destination. It stages source under ignored image/work,
applies reviewed non-browser PC adaptations, replaces display setup/session,
builds a native `.deb` in the target root, removes newly introduced compiler/dev
packages, retains runtime dependencies, and masks both original update services.
Rootfs check rejects browser/WebEngine packages and renders an offscreen demo.

PC keeps Debian 12 `linux-image-686-pae` + BIOS/IA32 UEFI or Debian 13
`linux-image-amd64` + BIOS/x64 UEFI. Persistent USB is supported by the inherited
design; no internal-disk installer exists. GRUB UUID discovery, safe graphics and
FAT diagnostics are retained. Secure Boot is not supported by these custom images.

Pi keeps the pinned official 2024-11-19 Pi OS Lite Bookworm firmware/kernel/layout,
with compressed hashes verified against official `.sha256` files. Kernel/package
security levels change through apt and must be recorded/reviewed before release.
No moving PiShrink download is used; native wrapper compression uses bounded
threads. Firmware/HAT and physical tryboot behavior remain hardware gates.

Outputs (not committed): `image/out/pitunes-native-{i386,amd64,armhf,arm64}.img.xz`,
SHA-256 and source/build labels. Current artifacts are development candidates.
There is no automatic release publication; native stable OTA, A/B bundle tooling
and legacy publication/deployment entry points are disabled.

## What these checks cannot establish

Staging tests run on macOS and check all four actual patch/overlay paths. Package
tests compile under the target userspace. Rootfs screenshots validate software
painting. None establishes a bootable image, physical input latency, DAC quality,
RAM targets, actual BIOS/UEFI compatibility, receivers, or power-loss rollback.
Run and retain the full boot/hardware matrix before publishing release claims.

## Observed development builds

All four image jobs succeeded at e428678. i386/amd64 immutable images passed
1 GiB QEMU BIOS offline GUI, IA32/x64 UEFI GUI/MPD, disabled update and bounded
display-failure/backend playback/report checks. ARM results are Pi OS rootfs
build/render checks, with no physical firmware boot. Artifact links, exact run
IDs, newer app-package fixes and outstanding gates are in PROGRESS.md.
