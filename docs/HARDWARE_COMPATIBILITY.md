# Hardware validation ledger

| Target | Source/build configuration | Physical native boot | Audio / input / resources |
|---|---|---|---|
| i386 Atom 1 GB | Debian 12 image built; BIOS + IA32 UEFI 1 GiB QEMU smoke passed | Owner confirmed USB boot | USB album browsing works; animation, Wi-Fi, built-in audio and trackpad problems reported. Retest fixes and collect measurements. |
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

## Owner-confirmed Atom N280 reference unit

Booted `antiX-26.1_386-full.iso` on the physical PC. The owner reports an
Intel Atom N280 (i686, one core with Hyper-Threading), 1 GB RAM (987.7 MiB
usable), and 1024×600 at 60 Hz. The Intel 945GSE/GMA 950 used kernel `i915` and
Xorg `intel`, with llvmpipe for OpenGL. antiX used kernel
`5.10.256-antix.1-486-smp`, 292.1 MiB RAM and 155 processes in the observed
session. The owner also reports roughly 300 MB for Windows 7 on the same PC.

| Hardware | Observed driver and details |
|---|---|
| Intel NM10/ICH7 HDA | `snd_hda_intel`, Realtek ALC272. ALSA card `Intel`, device 0, one shared analogue PCM for speakers and headphone jack. `plughw:CARD=Intel,DEV=0` retains the card identifier. |
| Analogue mixer | Master at 41%, Speaker and Headphone at 100%, all unmuted in antiX. Auto-Mute enabled. These are observations, not levels to initialise. |
| BCM4312 b/g LP-PHY | Broadcom STA `wl` was working in antiX. Compare the PCI ID reported by PiTunes before selecting an offline driver. |
| Realtek RTL810xE Ethernet | `r8169` in antiX. |

antiX uses a small window manager and a different init/service stack. Native
PiTunes uses Openbox without a compositor and retains Debian 12/systemd for
its service dependencies. Measure steady, equivalent workloads before drawing
a conclusion from the two desktop memory snapshots. `llvmpipe` is relevant
to the original WebGL runtime. QPainter still rasterizes covers on the CPU,
while the Xorg DDX handles screen copies and presentation.
[antiX architecture](https://antixlinux.com/about/)
[X.Org Intel acceleration matrix](https://www.x.org/IntelGraphicsDriver/)
