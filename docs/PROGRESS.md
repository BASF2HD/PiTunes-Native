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

Revised the projection after inspecting 40 consecutive frames of a 60 fps
reference transition and comparing ten timed native frames. At rest the first
side cover exposes about 63% of the centre cover's width. Its outer edge is
about 96% of the centre height and its inner edge is about 64%. Cover faces
and reflections now use the same perspective, with a small clearance between
the two passing faces. Navigation carries velocity on a path with continuous
spacing. Reflection overlap remains masked. The caption and drawer format
the date as `Album | Year`. Debug and Release passed 24 native cases, including
reflection masking, first-neighbour geometry and velocity continuity.

Measured the fast-scroll reference at 60 fps, or 16.7 ms between frames. In the
55–57 s segment of the supplied 11.34.11 clip, multiple albums cross the centre
quickly. The final blue cover enters at about 56.13 s. Its main turn slows past
56.4 s, and it continues drifting sideways as its front flattens from 56.53 s to
56.73 s before the next gesture. Native CoverFlow now tracks fractional deltas
and momentum on a frame timer that incoming packets cannot postpone. The centre
cover stays compact at speed and carries velocity into a slower final settle.
The release response increases for a steep approach so that the tested fast
gesture lands without reversing across the centre.
In an offscreen 1280×680 Mac playback of synthetic trackpad packets at 2× scale,
the median time to paint and grab a frame decreased from 21.1 ms to about 9 ms after
using a logical-resolution projection during motion. This measurement includes
the capture operation. Trackpad feel and appliance frame rate remain to be
verified on physical inputs.

The caption now follows the centred album while the user browses, returning to
three song metadata lines when they reach the playing album. The native chrome
adopts the NaviGlassPlayer control layout with repeat/shuffle at the top,
round play/pause, separate album navigation, a drawer button and icon menus.
The drawer uses system type and the reference's three-bar pulse for a visible
playing row. It stops pulsing when paused or when reduced motion is enabled.
All 26 native cases passed in Debug and Release with Qt 5.15.19 on macOS.
The regressions cover fast input continuing to paint, the album strip fetching
a later page without a playback mutation, caption context and the playing,
paused and reduced-motion markers.

Reviewed another 46 frames of the supplied 60 fps clip starting at 56.70 s.
The outgoing blue cover is visibly angled by 56.75 s. The incoming cream cover
is almost flat at 57.00 s and continues translating toward the centre. Native
projection now varies the turn by direction of travel, with matching endpoints
and face/reflection angles. Confirmed mirrored left/right motion and preserved
clearance as the two faces pass. Physical trackpad feel remains unverified.

The larger cover scene extends behind the transport. The album strip uses a
translucent track and a thumb inset from both guide lines. The square opaque
song drawer, overlay search and footer menus were checked with local artwork at
800×480 and the larger scene at 1280×680. The count appears inside the Albums
menu. Song and caption actions reuse the favourite and playlist APIs. The idle
return selects an already loaded playing album after five seconds and waits
while the user inspects the library. The native regressions cover the timer
through playback polls, the search geometry, six dropdowns, context actions,
and the optimistic heart state with API error recovery.
All 31 native cases passed in Debug and Release using Qt 5.15.19 on macOS.

The shorter transport and its footer tabs now sit over the cover reflections.
Covers keep their source edges, the track step buttons use round hover rings,
and the selected favourite heart fills white. The drawer, browse and action
menus, volume, search and metadata panels use blurred artwork sampled in Qt
at a bounded size. The drawer has square corners with the reference's faint
edge. Verified the artwork remains visible through player chrome using local
album covers at 800×480 and 1280×680.

More info follows the NaviGlassPlayer track and album layouts. The existing
album tracks API supplies per-track tags and album totals. Current playback
adds MPD bitrate only when status and track metadata share the same queue ID.
Fields without source metadata display Unknown. The running preview backend
can supply the new bitrate field on its next start.
All 33 native cases passed in Debug and Release with Qt 5.15.19. All 32 Python
cases and 24 JavaScript cases passed, including MPD bitrate pairing and the
existing playback modes.

Reviewed 53 consecutive frames of the supplied 60 fps clip beginning at
55.28 s. Several broad, nearly front-facing covers cross the centre during
the sustained swipe. The native fast path now holds a broader centre pass
and paces trackpad travel at up to 20 albums per second. It keeps the full
gesture destination and uses the same damped settle near the target. Checked
a 2× synthetic trackpad replay on macOS. Physical gesture feel is pending.

The drawer uses darker glass and a faint square border. Each song has a
favourite heart before its duration, and the row action menu omits Favourite.
Frameless popups clear the transparent corner pixels and use a rounded mask.
The taller album thumb nearly fills its translucent rail and scales its width
with the viewport. Application settings has a Font tab for title, artist or
singer, and album text. Its local preferences apply immediately and persist
across launches. Reviewed 800×480 and 1280×680 captures of the player, drawer,
rounded menu and settings.
All 35 native cases passed in Debug and Release with Qt 5.15.19. The
regressions cover the Font tab, row favourites without playback, thumb
geometry, rounded popup masks and the fast gesture destination.

Adapted the local NaviGlassPlayer runtime renderer's normalized 52-degree
side angle, edge pivot and cubic easing after reviewing `public/renderer.js`
and the wheel handler in `public/app.js`. Position, rotation and scale each
retarget from the current pose during input bursts. Qt still projects each
cover and its reflection together and keeps the renderer and artwork caches
bounded. The previous speed-dependent carousel is superseded. Local native
checks cover painting through input bursts, pose timing, neighbour reflection
masking and idle settling.

The footer is now centered within a 520 px menu row and a maximum 920 px
transport row. The transport and seek-to-cover gap are slimmer. Caption
defaults are 40/25/25 px. Songs use darker glass and closer heart/time
spacing, and the caption has its own contextual heart. Settings uses compact
tabs, a rounded shell, a title drag area and a top-right close button.

The Mac preview was playing through MPD's enabled `osx` output while macOS
was muted with the built-in speakers selected. Unmuted system output without
changing its volume. The Audio tab now shows the actual system output on
the local Darwin preview. Verified the built-in speaker selection in that
tab and that playback time advanced. Audible output remains unverified.
All 33 native cases passed in Debug and Release with Qt 5.15.19, including
the compact footer, settings drag and close, and caption favourite rollback.

Restored the 36-degree stack proportions and face/reflection projection from
`f3ab1ff`. Kept the latest trackpad retargeting and 420 ms glide with rotation
and depth settling together at 280 ms. Millisecond pose samples verify the
final approach is monotonic and lands on the exact resting quads. Physical
trackpad feel remains to be checked.

The song drawer uses available track credits. A normal rescan now reads embedded
M4A artists without changing browse album identity. The refreshed Mac preview
returned credits for all 14 tracks on the affected album with its original
numeric ID and preserved the paused queue and favourites. The narrower song
seek bar sends the clicked time, and the volume popup restores the previous
level after mute. Font controls have visible steppers and accept up to 80 px.
Settings uses the browse underline and corner radius. The Audio tab keeps the
appliance catalogue alongside the Mac output selector, with dark list popups.
Mac Qt 5.15.19 Debug and Release each reported 36 passing QtTest entries.
Python unittest discovery passed 33 tests and Node passed 24. The Mac output
list displayed all three output-capable devices reported by the host.

The playback row now uses 24 px controls and caps the song seek strip at
480 px, giving CoverFlow more vertical room. Settings uses a 4 px panel radius,
platform-neutral output labels and the shared scrollbar style. A click on
another footer menu or settings tab now switches directly through an open
popup. Local tests exercise all six menus, all Settings tabs and the handoff
from an open audio dropdown.

More → Radio opens a native search and saved-station browser. Radio Browser
supplies station data and logos. Only visible thumbnails are fetched, with
initials available when the upstream logo cannot be retrieved. Search paging
and Save reuse the existing API and station store. The Qt test exercises art
decode, saving independently of playback, and playing a saved station by ID.
The real HTTP test covers a cold logo fetch and a missing logo response.
The Mac preview was inspected with live BBC directory results. The isolated
HTTP handler saved a station, deduplicated it by directory ID and returned it
from saved stations. Qt 5.15.19 Debug and Release each reported 39 passing
entries. Python unittest discovery passed 34 tests and Node passed 24.

CoverFlow now switches between album, artist, song, playlist, search and saved
station shelves. Browse changes keep MPD’s queue independent. Saved stations are
played by ID from the cover double-click or station action. The centred library
search uses both result types. Trackpad wheel events stay inside scrollable lists,
track titles can be selected, the footer has a full-view button, and Settings can
browse folders on the server. The footer underline scales with the active tab and is inset from both edges. Album rail dragging uses direct selection while held.
NaviGlassPlayer’s independent position and rotation timing is applied to the
existing 36-degree geometry, with stable rail-based stacking.

Manual Next preserves backend album continuation. At the end of an eligible
queue, it resolves the next album using the ordering captured when playback
began. The Mac preview sample was a 320 kbps 44.1 kHz MP3 and MPD showed
44.1 kHz PCM. This verifies that example at the source and decoder level.
The actual ALSA/CoreAudio device path still needs physical format verification.
The real isolated Mac MPD preview passed manual Next across two albums and
played a saved test station by ID with elapsed time advancing. Qt Debug and
Release each reported 40 passing entries. Python passed 35 and Node passed 24.

The 1 GiB Intel test image was requested for source `290ed70`.
[Development image build](https://github.com/BASF2HD/PiTunes-Native/actions/runs/46058291937).
The same source’s Linux package CI showed the existing trackpad regression
test could finish its packet replay before Linux’s 33 ms paint timer. Extended
the burst to span several ticks without changing the application code in the
image.

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

See FEATURE_PARITY.md/KNOWN_ISSUES.md. Queue editor and full playlist edits, native CSV, smart builder/evaluator/
playback, radio favourites/removal/reconnect, scan progress, shared
context publishing/following, applied persisted preferences, touch focus/IME and
precise visual parity remain. Album continuation uses the sort and filter saved
at the start of playback.
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

## Intel Atom physical test follow-up

The owner confirmed the i386 development USB boots on the real 1 GB PC and
recognises the separate music USB. Photos indicate missing b43 ucode15,
ACPI firmware errors that did not prevent boot, and an unresolved playback 502.
The earlier browser dependency audit is independent of this result.
The 38161d0 i386 image passed the expanded 1 GiB native BIOS/UEFI, fault isolation
and remote web smoke test in [Actions](https://github.com/BASF2HD/PiTunes-Native/actions/runs/46063960115).

Implemented low-memory animation and performance monitoring, ALSA PCM discovery
and hardware mixer selection, firmware-aware Wi-Fi status and interface
discovery, libinput tap-to-click, and a boot report on the Mac-readable FAT
partition. Added synthetic HDA, legacy Atom, Wi-Fi and log redaction cases.
The current revision needs a fresh i386 development image and a physical retest
of audio, networking, touch input, fluid animation and monitor overhead.
The user guide includes per-machine b43 installation and an SSD installer design.
See [Atom hardware](ATOM_HARDWARE.md) and [Performance](PERFORMANCE.md).

Local verification for this milestone used macOS arm64 Qt 5 Release. The native
contract suite passed, including seek width/centre alignment, Lightweight
render sampling, monitor CSV and the disabled sampler. Python discovery
passed 40 tests, JavaScript passed 24 tests, and the release validator passed.
No physical performance metrics were collected in this workspace.

The first Linux package run for this change caught a Linux-only missing
`QCoreApplication` include in the monitor. It reproduced on both PC targets
and was corrected in a follow-up. The hosted backend and release checks passed.

The exact i386 development image at `448af5814f4bb4a62d9d5cf035fc8d64d1524835`
was built and uploaded in
[Actions](https://github.com/BASF2HD/PiTunes-Native/actions/runs/46090851382/artifacts/15241166985).
[Native CI](https://github.com/BASF2HD/PiTunes-Native/actions/runs/46090851398)
passed the backend and all four architecture package jobs. The Mac Release
and Debug suites each passed 41 Qt test entries. Controlled Mac frame and
monitor overhead measurements are recorded in PERFORMANCE.

On that image the first smoke run reached BIOS/UEFI boot and playback but
looked for the routine report too early on the forced-failure boot path. The
rerun checked the routine report during a healthy boot. It passed BIOS and
IA32 UEFI, USB playback through default and detected HDA analogue routes,
playback with forced Xorg failure, both FAT diagnostic reports, and native-only
local process inspection with remote browser assets served by nginx.
[Smoke evidence](https://github.com/BASF2HD/PiTunes-Native/actions/runs/46092554755/artifacts/15241698180).
The verified image's report can omit model strings containing commas or @.
A fixture-tested follow-up accepts that punctuation in the source.
