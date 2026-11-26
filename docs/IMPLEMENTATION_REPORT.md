# Native implementation milestone report

26 November 2026, PiTunes Native 0.1.0 development. Knowledge inheritance and the
first functional native appliance milestone are implemented and pushed. Full
feature parity, target performance and physical hardware acceptance are unfinished.

## Provenance and project safety

Read the supplied master instructions and full transfer. Inspected the original
local source, live GitHub main/history/releases/workflows/tests and original visual
reference. All 174 tracked original files match pinned dd34c4635a523b8dabc40e976f2d7b11abf88885;
final read-only hash verification found zero changes. Original local status still
contains only the pre-existing untracked transfer document. No unpublished source
or original runtime/credential data was imported, and nothing was pushed there.

Native repository branch is main, origin BASF2HD/PiTunes-Native. Latest runtime
source commit is 656eb46af5a350f7aaee9ab3660b8adf0190e96c, pushed successfully.
Later documentation commits do not change the image/runtime source identity.
No production release or native OTA is enabled.

## Files and modules delivered

| Location | Purpose |
|---|---|
| AGENTS.md | Permanent mandatory reading, original isolation and architecture/release rules |
| docs/PROJECT_CONTEXT.md, ARCHITECTURE_AUDIT.md, ARCHITECTURE_DECISIONS.md | Inherited contracts, source ownership and native choices |
| docs/KNOWN_ISSUES.md, FEATURE_PARITY.md, PROGRESS.md | Historical failures, honest native gaps, executed work and exact build evidence |
| docs/MASTER_DEVELOPMENT_INSTRUCTIONS.md, inheritance/, reference/ | Full brief, original hash inventory and inactive historical workflows |
| CMakeLists.txt, CMakePresets.json, native/ | C++17 Qt Widgets models, async networking, bounded artwork workers, raster CoverFlow and native controls/settings |
| backend/library/, backend/server.py | Proven backend retained; additive migration, native update isolation, bounded POSTs and year/RAM corrections |
| frontend/migrate.html | Explicit browser smart-playlist/preferences export; original browser data retained |
| scripts/*native*, systemd/pitunes-native.service | Native package/session/install checks and unprivileged bounded GUI recovery |
| tools/prepare-native-source.py, build-native-*, test-native-pc-image.py | Safe staging, architecture packages/images and immutable PC VM smoke tests |
| image/native-targets.json, .github/workflows/native-*.yml | Four target profiles, hosted CI, manually built development images/smoke artifacts |
| tests/native/, tests/test_native_*, tests/test_browser_migration.py | Native and real HTTP/staging/migration regressions |
| docs/BUILDING.md, DEVELOPMENT.md, HARDWARE_COMPATIBILITY.md, VALIDATION.md, PERFORMANCE.md, benchmarks/, validation/ | Reproduction, evidence, benchmark constraints and release gates |

## Implemented behavior

Native software-painted CoverFlow/reflections, bounded artwork caching/decoding,
mouse/wheel/keyboard/touch handlers, debounced album search, filters, paged albums,
track drawers, asynchronous serialized playback controls, seek/volume/repeat/
shuffle, basic favourites, playlist creation/browse, radio and settings forms.
Late API startup, stale navigation and destroyed async receivers have regressions.

Original MPD/ALSA audio/queue ownership, scanner/SQLite/JSON stores, storage/network/
receiver helpers, nginx and remote web UI remain. Explicit smart definition import
is additive and preserves unsupported fields. Local display needs no Chromium,
WebEngine or mandatory GPU acceleration. GUI failure leaves MPD/API independent.

## Tests and builds actually observed

- 32 selected Python tests and 24 JavaScript tests passed.
- Mac arm64 Debug/Release built; 17 substantive Qt cases passed in each (19 with
  init/cleanup). Includes 5,001-album paging, bounded jobs/cache, idle render/prefetch,
  stale-result/lifetime/timeout guards, SVG paint and native input/state contracts.
- Release validator and three historical shell updater/boot rollback suites passed.
  Native OTA remains disabled; historical fixture success does not validate it.
- Software screenshots at 800x480/480x800 and real Cocoa window inspected.
- Clang analyzed ten translation units, with three possible Qt closure ownership
  leaks reviewed; not a warning-free analyzer result or a target leak profile.
- GitHub CI compiled/tested/installed/rendered packages for all four architectures;
  final runtime source 656eb46 passed all five jobs in
  [run 45906727236](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45906727236). ARM packages use QEMU user emulation.
- All four development disk images built at e428678. Pi images use actual Pi OS
  Bookworm rootfs, firmware and verified official base hashes, not generic Debian.
- i386 and amd64 images each passed 1 GiB QEMU BIOS offline GUI/OCR and IA32/x64
  UEFI GUI/MPD silent-WAV playback; observed bounded LightDM failure, continued MPD
  playback and recovered FAT diagnostic report. Screenshots/result JSON inspected.
- **Physical native boots: zero on every architecture.** ARM rootfs rendering is
  not a Pi firmware boot; silent-WAV elapsed playback is not DAC quality evidence.

All image/artifact links and exact workflow IDs are in PROGRESS. Images precede
later UI fixes; application packages are validated separately without redundant
full OS rebuilds. Downloads are development artifacts, subject to GitHub retention.

## Measured performance and unresolved work

One 10-second Mac offscreen Release demo sample measured maximum process RSS
54.55 MiB, user CPU 0.09 seconds and system CPU 0.01 seconds. No audio/API/artwork
load or Chromium baseline; this is no target RAM, latency or performance claim.
Linux benchmark collection tooling and scenario guidance exist; actual identical-
hardware comparison remains undone.

Native code gaps include all-song paging (currently 200), queue editing, full
playlist management, CSV save, smart builder/evaluation/playback, radio paging/
favourites/reconnect, folder browsing, scan progress, shared context publishing/
following, full persisted preferences, touch focus/IME and exact appearance.
Backend continuation can use prior/default context rather than the native view.
Settings hardware actions and GUI health diagnostics need further verification.
These are explicitly tracked implementation tasks, not attributed to missing hardware.

Hardware blockers: actual Atom/Zero 2 W/Pi displays, touch, DAC/HATs and receivers
are unavailable. Cold boots, minimum CPU compatibility, constrained-device RAM/
responsiveness, long playback under scan, media removal, NAS/network recovery,
audio formats/routes/listening and power-loss behavior remain release gates.
Full Qt/vendor redistribution obligations and trusted-LAN API security also need
release review. Do not publish a stable OS or enable OTA before validation.
