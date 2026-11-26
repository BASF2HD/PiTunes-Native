# Hardware validation ledger

| Target | Source/build configuration | Physical native boot | Audio / input / resources |
|---|---|---|---|
| i386 Atom 1 GB | Debian 12 image built; BIOS + IA32 UEFI 1 GiB QEMU smoke passed | Not tested | VM silent-WAV only; physical input/resources untested |
| amd64 Intel/AMD | Debian 13 image built; BIOS + x64 UEFI 1 GiB QEMU smoke passed | Not tested | VM silent-WAV only; physical input/resources untested |
| armhf Zero 2 W / Pi 3 / 3 B+ | Pi OS Lite Bookworm image built | Not tested | Not tested |
| arm64 Pi 4 / Pi 5 | Pi OS Lite Bookworm image built | Not tested | Not tested |
| Development Mac arm64 | Qt 5.15.19, AppleClang 21, Debug + Release | Application only | Offscreen mock/demo rendering and tests; no target audio |

Debian 12 i386 has an i686 baseline; the chosen 686-pae kernel requires PAE.
The old SSE3 constraint included Chromium and is not a native application build
requirement. The actual Debian 12 Qt 5.15.8+dfsg-11+deb12u3 `debian/rules` was read
from the [official source archive](https://deb.debian.org/debian/pool/main/q/qtbase-opensource-src/qtbase-opensource-src_5.15.8+dfsg-11+deb12u3.debian.tar.xz):
its i386 baseline is configured with `-no-sse2 -no-pch`, and separate Core/GUI SSE2
variants are built into the architecture's sse2 directory. Native 32-bit x86 CMake
uses `-march=i686 -mtune=generic -mfpmath=387`. This removes a browser-derived SSE3
assumption; it does not validate the whole OS/graphics stack on an actual CPU.
Non-PAE boot is not supported by the chosen kernel. See
[Debian 12 i386 release notes](https://www.debian.org/releases/bookworm/i386/release-notes.en.pdf).
[Qt 5.15 Linux documentation](https://doc.qt.io/archives/qt-5.15/linux.html) lists
generic x86 support; it is not a model-specific hardware validation certificate.

Retest absent NIC/controller/USB music, missing NAS, repeated cold boot, disk
enumeration with an internal Windows disk present, graphics fallback, awake
display while paused, actual touch/keyboard, USB DAC reboot ordering, Pi HAT round
trips, network recovery/receivers and long playback under scan load. Never modify
the owner's internal disk to diagnose this project.
