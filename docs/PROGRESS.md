# Progress

26 November 2026 (Europe/London), main, native version 0.1.0 development.
This is a functioning first native appliance milestone, not full feature or
physical-hardware acceptance. Read AGENTS.md before continuing.

## Inheritance completed

Read the full master brief and all 1,267 transfer lines. Audited original local
source read-only, Git history, live GitHub tree/releases/Actions, documentation,
API/store/scanner/audio/boot/build/tests and visual baseline. Original HEAD/live
main is dd34c4635a523b8dabc40e976f2d7b11abf88885. All 174 tracked blob IDs and
initial destination files match. No unpublished source imported. Only original
nonignored untracked file is the supplied transfer document; ignored runtime data
was excluded. Original unchanged; SHA-256 inventory retained.

Created permanent AGENTS.md and context/audit/decisions/issues/parity docs. Native
product/version/channel isolated; native OTA fails closed before network or
privileged work. Original image/release workflows archived inactive. All milestone
commits pushed only to BASF2HD/PiTunes-Native/main; no original remote here.

## Implemented

- CMake/C++17 Qt 5 Widgets client, QPainter cover projection/reflections, native
  input, timed navigation stopping at idle, tested 5,001-album paging, async API
  adapter, stale-response guards, virtual track lists, transport/seek/volume/
  repeat/shuffle, filters/search, basic favourites/playlists/radio and appliance
  settings. No browser/WebEngine/OpenGL dependency in the local UI.
- Artwork byte-budgeted cache/reflections, two decoding jobs, pixel/byte limits,
  bounded prefetch avoiding idle eviction loops. Qt SVG Tiny paint adaptation
  preserves generated cover colours; optional dotted texture is omitted locally.
- Original Python/MPD/ALSA/SQLite and remote frontend retained. Fixed available-RAM
  report column; exposed existing song year in player state. No new audio engine.
- Explicit browser export/shared-store import preserves smart definitions and
  safe preferences without deleting originals or executing unsupported rules.
- Minimal X11/LightDM session, unprivileged systemd user GUI, bounded recovery and
  awake-display commands, independent MPD/API. Legacy browser entry points replaced.
- Four source-staging/PC patch paths tested, pinned architecture CI and manual
  image/smoke workflows, source/package/checksum manifests. Safe staging copies
  only Git-visible approved paths, rejects symlinks/keys and excludes ignored envs.
- i386 explicitly i686/x87; Debian 12 Qt non-SSE2 baseline reviewed. PAE kernel
  requirement remains; actual minimum CPU/graphics needs physical validation.

## Executed source/application validation

- Python: 32 passed. JavaScript: 24 passed. Historical release validator and three
  updater/boot rollback shell suites passed; these do not approve native OTA.
- Mac arm64 Debug/Release compiled; 17 substantive Qt tests passed in each
  configuration (19 including init/cleanup). See VALIDATION.md for coverage.
- Offscreen demo/mock screenshots at 800x480 and 480x800 inspected; real Cocoa
  window also launched/captured. Low-resolution keyboard/modal QA remains.
- Clang analyzer processed ten translation units; three possible Qt functor
  ownership leaks reviewed, not claimed warning-free. Linux profiling remains.
- One measured 10-second Mac offscreen demo sample: 54.55 MiB maximum process RSS,
  no audio/API load; docs/benchmarks/macos-demo-process.json. No target comparison.
- Final architecture CI at 656eb46 passed all five jobs: backend and all four
  packages, including the 17-case Qt suite, DEB installation and raster PNG:
  [run 45906727236](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45906727236).
  ARM package execution uses QEMU userspace, not physical Pi hardware.

The nearest side covers now overlap the centre cover slightly, closing the gap.
Checked the seam in an 800×480 Mac window and tested click selection just outside
both centre edges. The regression failed with the old geometry and passed in
both Debug and Release after the adjustment.

CoverFlow now derives the visible stack from viewport width, cover size and the
artwork cache budget. Covers and reflections share a baseline. Mac mock renders
showed 9 covers at 480×800, 15 at 800×480 and 19 at 1440×520, with the same album
selected. Debug and Release passed the 18-case suite, including the resize and
rendered baseline regression. Physical-device frame rate remains unmeasured.

Nearer CoverFlow reflections now mask farther reflections across their footprint.
A pixel test reproduced colour mixing on the old renderer and passed on both
sides after the change. Debug and Release passed all 19 native cases. The fix
was also checked with local album artwork.

27 November: removed the player header logo and consolidated playback metadata
under the centre cover. The caption shows the track, singer and album/year even
while browsing other albums. The track drawer follows the PiTunes and
NaviGlassPlayer layouts with a centred overlay, song count, numbered rows, singer,
duration, current-track marker and action menus. Full album queue order and file
identity are retained. Studied the supplied CoverFlow recordings and frame sheet
to tune slide, rotation and scale independently. Wheel input accumulates small
deltas and supports retargeting in flight. Checked the drawer at 1280×680, 800×480
and 480×800 and inspected a native transition frame sequence. Debug and Release
passed all 22 native cases. Hardware motion quality remains unmeasured.

## Image and boot evidence

Images are immutable development artifacts from e428678, preceding later UI fixes;
those fixes ship in independently validated app packages. Do not rebuild an OS
just for ordinary UI changes. No stable release or native OTA was published.

| Target | Image build | VM boot | Physical boot |
|---|---|---|---|
| amd64 | [45903207496](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45903207496) succeeded | Enhanced [45905118978](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45905118978) succeeded | None |
| i386 | [45904926832](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45904926832) succeeded | [45906015607](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45906015607) succeeded | None |
| armhf | [45904928762](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45904928762) succeeded | Not Pi firmware validation | None |
| arm64 | [45904930633](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45904930633) succeeded | Not Pi firmware validation | None |

amd64 image includes Debian 13 BIOS/x64 UEFI, installed native package, no browser,
removed build tools, rootfs demo PNG, nginx/bootloader checks and verified xz/SHA.
Artifact 15157350867 (~1.35 GB ZIP) contains pitunes-native-amd64.img.xz, checksum
and build label. Enhanced smoke used a 1 GiB QEMU guest: no-NIC BIOS native GUI/OCR,
x64 UEFI native GUI, MPD silent-WAV elapsed playback, disabled OTA, observed bounded
LightDM failure, continuing MPD playback after display failure and extracted FAT
diagnostic report. Both PC image smoke suites passed, including i386 BIOS/IA32 UEFI at
[45906015607](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45906015607).
Both architectures' VM screenshots were inspected. This establishes neither DAC output
quality nor physical responsiveness, RAM targets or a Chromium performance gain.

## Lessons from actual failures

First package CI at f538876 compiled/tests passed on x86 but CPack failed: minimal
containers lacked `file` for shlibdeps. Added it to package/image build tools;
superseded ARM/image work cancelled. Run 45902512642 then passed all four packages.
Image 45902534773 next compiled/installed but failed the executable-path check:
DEB /usr differed from launcher /usr/local. Made CPack /usr explicit, aligned
launcher/checks and added installed-package execution to every architecture job.

Delayed API/empty first scans now retry with bounded backoff. Follow-up regressions
cover stale drawer navigation, callback destroying its ApiClient, initial shuffle
state and ignored local environment exclusion. SVG colours were corrected after
inspecting actual VM screenshots; backend/remote output remains intact.

## Remaining implementation and release gates

See FEATURE_PARITY.md/KNOWN_ISSUES.md. All-songs browse currently first 200; queue
editor, all-song paging, full playlist edits, native CSV, smart builder/evaluator/
playback, radio paging/favourites/reconnect, folder browsing, scan progress, shared
context publishing/following, applied persisted preferences, touch focus/IME and
precise visual parity remain. Missing native browse context can change backend
automatic continuation's sort/filter. These are code gaps, not hardware blockers.
Settings forms and GUI-process health integration require further work/verification.

No physical native boots or target performance/audio measurements. Actual 1 GB
Atom/512 MB Zero 2 W resources, USB/HAT/DAC routes, formats, long playback under
scan, touch, offline media, NAS/network/receivers and power-loss recovery remain
release gates. Target hardware is unavailable here; GitHub root Linux builds and
PC VM tests are available and have been used. Do not publish production images or
enable native OTA before those gates pass.

## Development image artifacts

All four are e428678 source, developmental, with no physical boots. Each artifact
contains a compressed image, SHA-256 and build label. GitHub retention applies.

| Target | Artifact | ZIP bytes |
|---|---|---|
| i386 | [15157039425](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45904926832/artifacts/15157039425) | 1,151,405,441 |
| amd64 | [15157350867](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45903207496/artifacts/15157350867) | 1,347,089,797 |
| armhf | [15158406009](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45904928762/artifacts/15158406009) | 1,123,920,201 |
| arm64 | [15156994546](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45904930633/artifacts/15156994546) | 1,115,439,553 |

Both Pi builders verified official base digests, compiled/installed the native DEB
in the Pi OS rootfs and passed the no-browser/offscreen software-render check.
Their firmware boot, HATs and displays remain untested. Inherited Pi build output
still mentions a Chromium splash and legacy publish command; these are stale log
messages, not installed browser packages or authorization to publish a release.
