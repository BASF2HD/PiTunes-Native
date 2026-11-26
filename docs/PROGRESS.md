# Progress

26 November 2026 (Europe/London), main, native version 0.1.0 development.

## Completed

- Read the complete master brief and all 1,267 transfer lines. Audited original
  local source read-only, Git history, live GitHub tree/releases/Actions,
  documentation, API/store/scanner/audio/boot/build/test paths and visual baseline.
- Original HEAD/live main dd34c4635a523b8dabc40e976f2d7b11abf88885. All 174 tracked
  blob IDs and initial destination files match; no unpublished source imported.
  Only original untracked file is the supplied transfer document. Original status
  checked again after implementation: unchanged. SHA-256 inventory retained.
- Foundation commit 86a5208 pushed successfully to BASF2HD/PiTunes-Native/main.
  Native product/version/channel isolated, all native OTA paths fail closed before
  networking or privileged work. No original remote was configured here.
- Added AGENTS.md, inherited context/audit/decisions/issues/parity and current docs.
  Original image/release workflows archived inactive, old docs explicitly labeled.
- Implemented CMake/C++17 Qt 5 Widgets client, QPainter cover projection/reflections,
  native input, time-based navigation stopping at idle, 5,001-album tested paging,
  async same-origin API/error/timeout adapter, stale generation guards, virtual
  track lists, transport/seek/volume/repeat/shuffle, filters/search, favourites,
  playlist creation/browse, basic radio and async initial appliance settings.
- Artwork cache is cost-bounded with two jobs, scaled worker decoding, rejected
  corrupt/oversized inputs, revision URLs, cached reflections and a prefetch window
  that fits its memory budget (regression-tested against idle eviction/refetch loops).
- Explicit browser export/import preserves smart definitions and safe preferences
  in existing userdata, without deleting browser data or executing unsupported
  rules. Native builder/evaluator remains pending.
- Added native X11/LightDM session and bounded systemd user GUI restarts, separate
  from MPD/API. No root GUI. Original browser entry points disabled/replaced.
- All four native source-staging/PC patch paths tested. Added independent pinned
  architecture package CI and manually dispatched image workflow, source/checksum
  manifests, official Pi compressed-base checksums, rootfs no-browser/raster checks.
  Actual native images are not yet built or boot-tested.
- Inspected Debian 12 Qt source rules: non-SSE2 baseline plus optional SSE2 libraries.
  i386 app explicitly targets i686/x87, kernel still requires PAE. Whole-image
  minimum CPU/graphics acceptance remains untested.
- Fixed inherited MemAvailable report column with a regression fixture and retained
  year metadata in native player state. Original audio/queue engine is retained.

## Executed validation

- 31 Python tests passed; 24 JavaScript tests passed.
- Debug and Release Mac arm64 compiled; CTest 13 substantive Qt cases passed
  (15 QtTest results including initialization/cleanup).
- Release validator + three historical updater/boot rollback shell suites passed.
- Offscreen demo/mock screenshots inspected, including 800x480 and 480x800.
- Clang built-in analyzer processed ten translation units; three possible closure
  ownership/leak warnings reviewed as Qt lifetime-model limitations. Not claimed
  warning-free; see VALIDATION.md. Linux allocator profiling remains pending.
- Native image wrapper correctly refuses unsupported Mac execution.
- High-confidence staged credential scanner and diff review required before commit.

## Current limitations / next work

See FEATURE_PARITY.md and KNOWN_ISSUES.md. Native all-songs browsing initially
caps at 200. Queue editor, all-song paging, complete playlist edits, CSV export,
smart builder/evaluator/playback, radio paging/favourites/reconnect presentation,
folder browser, scan progress, shared remote context following, complete persisted
preferences, touch focus/IME and detailed visual parity remain. Native settings
forms require real hardware/privilege verification. GUI-failure diagnostics beyond
existing X-server/FAT reports need an appliance health integration.

This Mac has no root Linux loop mounts/systemd/MPD/DAC or target hardware. CI
package artifacts are application validation, not Raspberry Pi firmware boots.
No target performance numbers or physical native boots exist. Four image builds,
BIOS/UEFI acceptance, actual 1 GB Atom/512 MB Zero 2 W resources, USB/HAT audio,
network/NAS/receiver behavior and power-loss recovery remain release gates.

Never publish production images or enable native OTA yet. Continue through
AGENTS.md and these docs; do not re-audit or modify the original repository.

## First Linux CI observation and correction

[Run 45902198098](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45902198098)
at f538876 compiled and passed CTest for both amd64 (Qt 5.15.15/GCC 14) and
i386 (Qt 5.15.8/GCC 12). Packaging then failed because CPack's shlib dependency
scanner needs the `file` utility, absent in minimal containers. Added it to both
package and image build dependencies. The backend job passed. Superseded ARM
package and first amd64 image runs were cancelled to avoid repeating the same
known packaging defect. A GNU misleading-indentation warning was also corrected.

A real Cocoa Mac window was launched and captured successfully (beyond offscreen
tests). A single 10-second offscreen demo process sample is recorded in
docs/benchmarks/macos-demo-process.json: 54.55 MiB maximum process RSS, no audio/API
load. It is not target system RAM, a Chromium comparison or appliance acceptance.

## Package installation / startup corrections

The corrected amd64/i386 packages built and passed CTest in run 45902512642.
Image run 45902534773 compiled/packaged/installed the app, then stopped at its
executable-location check: the DEB uses /usr while the session expected /usr/local.
Made /usr explicit in CPack, aligned session/checks to /usr/bin/pitunes-native,
and extended architecture CI to install each package and save a raster screenshot.
Added backoff retry for delayed API startup and still-empty first scans/mounts,
with a new late-backend test. Qt suite is now 14 substantive cases (16 including
init/cleanup). No false claim of a completed/booted image was made for these runs.

A separate manual Native PC image smoke workflow reuses the inherited disposable
QEMU machinery, with a 1 GiB guest, so an existing image artifact can be boot-tested
without rebuilding it. Includes no-NIC BIOS GUI, UEFI GUI/silent WAV, disabled
updates and backend playback while Xorg is intentionally unavailable. Not yet run.

## Four-architecture package milestone verified

[Run 45902512642](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45902512642)
completed successfully for backend plus all four architecture package jobs at
19e826e. Artifacts include .deb, checksums and exact build package manifests.
These packages are application builds; image runtime-path checks were improved
subsequently in e428678. Latest package CI additionally installs each .deb and
renders a demo screenshot. New amd64 image candidate run 45903207496 is in progress.

## Latest architecture runtime verification

[Run 45903161166](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45903161166)
at e428678 completed all five jobs successfully: backend plus i386, amd64, armhf
and arm64. Each target compiled, passed the 14-case Qt suite, generated a DEB,
verified the /usr/bin entry, installed it and rendered a software demo PNG.
Artifacts contain package, screenshot, checksums and build dependency manifest.
ARM results use QEMU userspace emulation, not physical Raspberry Pi hardware.

## First native disk image built

[Native amd64 image run 45903207496](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45903207496)
completed successfully at e428678. Built Debian 13 BIOS/x64-UEFI disk image,
compiled/installed native executable, removed build dependencies, verified no
browser package, rendered the rootfs demo, checked nginx and bootloader files,
verified xz integrity and generated SHA-256. Development artifact ID 15157350867
(about 1.35 GB ZIP) includes pitunes-native-amd64.img.xz plus checksum/build label.
This is not a stable release and was not called bootable merely because it built.
[1 GiB VM smoke run 45904112664](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45904112664)
was dispatched against that immutable image artifact and is being followed.

## First native VM boot/playback passed

[Smoke run 45904112664](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45904112664)
passed against the e428678 amd64 image: 1 GiB VM, BIOS offline native GUI/OCR,
UEFI native GUI and silent-WAV MPD elapsed playback, disabled native OTA, backend
playback with forced Xorg failure configuration. This is a virtual-machine smoke
result, not physical hardware/audio-quality/performance acceptance. Extended the
failure test further to assert actual failed LightDM state and extract the FAT
diagnostic report, rather than relying on fault injection alone; rerun pending.
After this first working Linux target, dispatched i386/armhf/arm64 image builds
from the same native implementation. No stable release/OTA was published.

## Follow-up source hardening (application packages, no OS rebuild)

Added tested guards against an album drawer opening from a response received
after navigating away, and cleanup ordering when an API callback destroys its
client. Shuffle now reflects the initial backend state on its first poll.
Debug/Release Qt suites now cover 16 substantive cases (18 including init/cleanup).
Hardened image staging to copy only Git-visible files in approved roots and reject
symlinks/key paths; an ignored local .env regression fixture is never imported.
Python suite is now 32 tests. Generated image candidates remain pinned to e428678;
these follow-up UI corrections ship as independently built application packages.
No full OS rebuild is needed just to validate these C++ changes.
