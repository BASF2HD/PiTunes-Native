# PiTunes Engineering Knowledge Transfer

Prepared: 2026-11-26, Europe/London.

Source project: <https://github.com/BASF2HD/PiTunes>

Destination project: <https://github.com/BASF2HD/PiTunes-Native>

Source workspace: `/Users/arvraju/Documents/PiTunes`.

Destination workspace: `/Users/arvraju/Documents/PiTunes-Native`.

Source baseline: `dd34c4635a523b8dabc40e976f2d7b11abf88885` on `main`, application version `1.4.4`.

## How to Use This Document

This is an implementation handoff, not a claim that every existing component is production-proven on every device. The native project is intended to replace the **local Chromium interface** with **C++17, Qt Widgets and QPainter**, while retaining the backend, audio engine, library behavior and appliance services where they actually work.

Evidence labels used throughout:

- **SOURCE**: verified against the checked-out implementation at the baseline above.
- **TESTED**: supported by named automated tests or a specifically identified completed workflow. Passing tests only establish what those tests exercise.
- **USER-REPORTED**: observed by the owner on physical hardware in this conversation, not independently instrumented here.
- **HISTORY**: supported by available conversation or Git history, not necessarily a complete forensic diagnosis.
- **RISK / UNRESOLVED**: an implementation gap, contradiction, missing measurement or behavior requiring further verification.
- **RECOMMENDATION**: proposed native-project work, not an already implemented feature.

Where documentation disagrees with source, the source at the recorded commit takes precedence. Where a VM passes but a physical PC is unusable, the physical performance report takes precedence for that target. There is no basis for a claim of universal hardware compatibility, a guaranteed few-second boot, bit-perfect audio, or a fully validated power-loss-safe OTA system.

All paths below are repository-relative unless they start with `/`. Runtime paths refer to the appliance, not the development Mac. No passwords, tokens, private keys, Wi-Fi secrets or NAS credentials are reproduced here.

## Contents

1. Current architecture
2. Feature inventory and ownership
3. User preferences and non-negotiable requirements
4. Architectural decisions and their rationale
5. Historical problems and fixes
6. Failed approaches and lessons
7. Raspberry Pi compatibility
8. Intel and AMD PC compatibility
9. Intel Atom and low-memory performance
10. Boot optimization and startup
11. Graphics and display recovery
12. MPD, ALSA and output configuration
13. Audio quality and playback correctness
14. Bluetooth and AirPlay
15. Wi-Fi, hotspot and NAS
16. USB and internal storage
17. SQLite schema and persistence
18. Scanning, metadata and artwork
19. Backend API reference
20. UI, CoverFlow and interactions
21. Settings, favourites, playlists and export
22. OTA, rollback and recovery
23. Security
24. Raspberry Pi image pipeline
25. PC image pipeline
26. GitHub Actions and build lessons
27. Current bugs and unresolved issues
28. Features that must survive migration
29. Source navigation map
30. Native migration strategy
31. Components to reuse
32. Components to redesign
33. Tests and release gates
34. Local changes and transfer boundaries
35. Current source, builds and releases

## 1. Current Architecture

### 1.1 Runtime topology

**SOURCE**:

```text
Local touchscreen / HDMI display         Other browser on trusted LAN
              |                                      |
        Chromium kiosk ------------------------------+
              | HTTP :80
            nginx
              | /api/* -> loopback :8080
      Python ThreadingHTTPServer
          |                |                    |
          |                |                    +--> restricted sudo appliance helpers
          |                +--> SQLite index, JSON settings/userdata, artwork cache
          +--> MPD loopback :6600
                     |
                   ALSA --> built-in output / HDMI / USB DAC / Pi DAC HAT

NetworkManager --> Ethernet / Wi-Fi / recovery hotspot
udev + systemd --> music mount and background rescan
Shairport Sync --> ALSA default audio
BlueALSA + bluealsa-aplay --> ALSA default audio
Avahi --> pitunes.local and service advertisement
```

The Python service uses the standard library HTTP server, not Flask or FastAPI. `backend/server.py` owns the route dispatcher. `backend/mpd_client.py` provides thread-local persistent MPD connections, command parsing, binary artwork reads and reconnect/retry handling. SQLite supplies browse/search results; MPD owns actual native local playback and radio decoding.

The local UI is a client, not the audio engine. It must be replaceable without requiring the display process to decode local music. In the browser-output mode only, an HTML audio element plays files through `/api/stream` on that browser's machine.

### 1.2 Ownership and filesystem layout

| Path | Purpose / authority |
|---|---|
| `/opt/pitunes` | Installed versioned application, frontend, backend, runtime scripts and config templates |
| `/etc/pitunes/settings.json` | Appliance settings persisted as JSON |
| `/etc/pitunes/userdata.json` | Favourites, saved playlists, saved radio stations |
| `/etc/pitunes/network-storage.json` | NAS configuration; sensitive contents, do not export |
| `/etc/pitunes/network-storage.credentials` | CIFS mount credentials; sensitive, do not export |
| `/var/cache/pitunes/library.db` | Rebuildable SQLite library index plus duplicated app settings |
| `/var/cache/pitunes/art` | Generated/cached album thumbnails |
| `/var/cache/pitunes/ui-context.json` | Shared browse and playback context |
| `/mnt/music` | Active library mount presented to MPD and scanner |
| `/var/lib/pitunes/music` | Internal library bind-mounted to `/mnt/music` when selected |
| `/var/lib/mpd` | MPD database, queue/state, playlists, stickers |
| `/var/lib/pitunes/update-status.json` | Persisted update state for clients |
| `/var/backups/pitunes` | App OTA rollback archives |
| `/var/log/pitunes/playback.log` | Backend continuous-playback diagnostics |
| `/var/log/pitunes-update.log` | App updater diagnostics |
| `/var/log/nginx/pitunes.*.log` | HTTP access/error diagnostics |

`backend/shared.py` exposes `MUSIC_DIR`, `PITUNES_CONFIG_DIR`, `PITUNES_CACHE_DIR`, `PITUNES_API_HOST`, `PITUNES_API_PORT`, `MPD_HOST`, `MPD_PORT`. Default API bind is `127.0.0.1:8080`, MPD `127.0.0.1:6600`. Some helpers have hard-coded runtime paths despite these environment overrides; do not assume all modules are relocatable.

### 1.3 Important missing integration

**SOURCE / UNRESOLVED**: `server.py` optionally imports `input_sources`, but no `backend/input_sources.py` is tracked at this baseline, and the inspected history did not establish such a module. The import failure is handled by setting all source-manager hooks to `None`. AirPlay and Bluetooth receiver services exist, but full source arbitration, receiver now-playing metadata, auto-pausing MPD and unified external transport must not be described as verified implemented behavior. See section 14.

## 2. Feature Inventory and Ownership

| Feature | Implementation | Qualification |
|---|---|---|
| Local playback | `backend/playback.py`, MPD, ALSA | Reuse rather than replace with Qt Multimedia by default |
| Album browsing | `library/queries.py:list_albums`, scanner identities, UI pagination | Numeric album IDs are authoritative |
| Song browsing | `list_all_tracks`, grouped/individual frontend views | UI initially limits loaded songs; must not mistake loaded rows for total |
| Artists/composers | `list_artists`, `list_composers`, album filters | Artist browse primarily follows album artist/display artist |
| Years/genres/rating | Album query filters and More menu | Rating data exists, but no general rating-edit API was found |
| Library search | SQLite FTS5 plus fallback query paths | Not just a filter of already loaded covers |
| CoverFlow | `renderer.js`, `coverflow.js`, Three.js, GSAP | Must be reimplemented with CPU-conscious native painting |
| Songs drawer | `app.js` drawer state, caches, context actions | Must retain album/track identity and instant playback |
| Favourites | `library/userdata.py`, optimistic UI | JSON persistence, not SQLite-only |
| Regular playlists | JSON `trackIds`, API create/add | No rename/delete/reorder route at this baseline |
| Smart playlists | Browser localStorage, rule evaluation in `app.js` | Not backend-managed or shared between devices |
| Repeat/shuffle | MPD options or browser `playback-order.js` | Separate engines, shared user semantics |
| Continuous albums | Backend MPD idle watcher plus backup poller | Must work with no UI connected |
| Radio | Radio Browser proxy, saved JSON stations, MPD URLs | Internet required for directory/streams; streams can change |
| CSV export | `music-export.js`, UI export modal | Read-only; deduplicates across selected sources |
| Artwork | `art_resolver.py`, API thumbnail and SVG fallbacks | Identity and cache invalidation are critical |
| Source selection | Settings, filesystem roots/browse, mount helper | Local USB, internal storage, SMB/NFS NAS |
| ALSA output picker | `audio_output.py`, privileged shell helper | Pi and PC paths differ materially |
| Wi-Fi/hotspot | NetworkManager, backend worker, hotspot watcher | Optional for offline PC playback |
| Bluetooth input | BlueZ, BlueALSA A2DP sink, pairing helpers | Receiver exists; unified source metadata/control incomplete |
| AirPlay input | Shairport Sync, ALSA, Avahi, optional nqptp | Installed capability is package-dependent; do not assume AirPlay 2 |
| System settings | service control, About, reboot/shutdown | Privileged operations via existing helpers |
| App OTA | release-based transactional shell updater | Pi public path, separate from OS upgrades |
| Curated system OTA | Signed A/B runtime and tools | Staging/capability-gated, not approved public PC OTA |
| Image builds | Pi and PC builders/workflows | Separate architecture-specific artifacts |
| Local mock UI | `tools/mock-server.py` | Does not validate real audio or appliance boot |

## 3. User Preferences and Non-Negotiable Requirements

**USER-REPORTED / explicit requirements**:

1. Preserve the working Raspberry Pi edition. PC or native work must not destabilize its ARM kernel, image, playback or update channel.
2. Treat this as a serious open-source music appliance, not a disposable prototype. Clear filenames, maintained documentation, licenses, diagnostics and repeatable releases matter.
3. Boot directly into the usable player without asking for a login. Fast boot is a goal, not a verified numerical promise.
4. Keep the display awake at all times, including idle and paused states. No screensaver, blanking, DPMS power-off or sleep during ordinary use.
5. Offline local playback is primary. No Wi-Fi, Bluetooth, Ethernet, NAS or Internet requirement to start the appliance and play local music.
6. Wireless receiver services and Samba sharing should be optional on PCs. NAS client use is not the same thing as running a Samba server.
7. Support older Intel/AMD PCs, including a 32-bit machine with only 1 GB RAM. The current Chromium build is not acceptable on the reported machine.
8. Preserve the classic CoverFlow feel, album reflections, readable centered metadata, efficient song drawer and touch/mouse/keyboard interaction.
9. Use plural navigation labels such as Albums and Artists. The current frontend still has a visible singular Playlist label in some surfaces; preserve intent rather than perpetuating that inconsistency.
10. Correct album grouping and cover matching are fundamental. One song must not become one album solely because its singer differs.
11. Never restart a song in the middle merely because browse state, scans or queue completion changed.
12. Preserve track artist independently of album artist; show song title, track artist and album/year distinctly.
13. Maintain USB DAC support and Pi-specific I2S HAT selection only where hardware supports it. PC edition must not offer Pi overlays as if usable.
14. OTA must show progress, lock conflicting actions, warn not to remove power, retain logs, and recover to the prior working version on supported failure paths.
15. Use GitHub-hosted builds. Do not switch to self-hosted runners for this work.
16. Keep the favicon/icon black-and-white rather than the earlier gray background variant. Reuse branding assets, not an improvised replacement.
17. Explain build state honestly. Source pushed, image built, tests passed, artifact available, release public, flashed and hardware-usable are different milestones.
18. Keep original and native repositories independent. Do not accidentally direct native users at PiTunes' original App OTA channel.

## 4. Architectural Decisions and Rationale

### 4.1 Separate playback from presentation

MPD owns queue playback so the browser can close or restart without stopping native audio. SQLite indexes the library so frontend requests do not repeatedly traverse MPD's whole database. Python and small systemd services were chosen over a large application framework for a lightweight appliance.

The Qt migration should strengthen this separation: one API-facing model/controller, one native presentation layer, existing backend audio authority. Avoid introducing a second independent local playback queue.

### 4.2 Separate architecture artifacts

Pi OS ARM images cannot boot Intel CPUs. The PC edition therefore builds Debian `amd64` and `i386` images separately. The Pi ARM builders remain unchanged. PC overrides are applied to a staging copy using `image/pc/*.patch`, rather than editing Pi source paths directly.

This isolation prevents Pi regressions, but creates maintenance cost: source changes can invalidate patches and patch order matters. A future architecture/capability abstraction can replace patches only with cross-platform contract coverage.

### 4.3 Release-based updates, not main tracking

Stable devices follow versioned releases resolved to immutable source commits. `main` is not a deployment channel. The original project has a real App OTA dependency on GitHub's Latest stable release, so changing release flags is operational, not cosmetic.

### 4.4 User data is not the library cache

SQLite is rebuildable but contains some app settings. Favourites, playlists and radio are in a separate JSON store. Browser-only preferences and smart playlists are in localStorage. Backing up only `library.db` is insufficient; wiping the browser loses data that was never in the backend.

### 4.5 Boot first, peripherals later

nginx/API/MPD and display must not wait for DHCP, Wi-Fi association, NAS, Bluetooth or SMB discovery. PC recovery disables receiver/server services by default. A network source selected by the user can still be unavailable; this must not prevent the native shell from opening and allowing another source.

### 4.6 Native direction is a new decision, not an existing implementation

C++17, Qt Widgets and QPainter are the user's new project requirements. No Qt application, CMake build, native executable, Qt packaging or native OTA support exists in the current repository. Do not mistake recommendations in sections 30-33 for implemented work.

## 5. Historical Problems and Fixes

| Problem | Evidence / change | What must carry forward |
|---|---|---|
| Screen powers off when idle or playing | `52adb03`, `12721cc`; kiosk launch and display scripts disable X blanking/DPMS | Always-on display independent of playback state |
| Wrong album cover vs song drawer | Conversation; album-ID/revision lookup, normalization helpers | Bind cover, metadata and tracks to same stable identity, never only array index |
| Unknown albums / one cover per song | Folder grouping work, `bc5e280`, scanner fallback | Folder/album metadata grouping, Various Artists compilation handling |
| Same album title incorrectly merged across releases | `b06b8e5`, backend identity and `album-identity.js` tests | Title + album artist + year; numeric ID wins |
| Slow song start caused by whole-library queue work | `f2f5de7`, `d9892ec`, fast-start backend | Start selected track first, finish queue asynchronously without restart |
| Cross-album playback stops or requires an open UI | `68e22e2`, `5ae34d6`, `d9892ec` | Backend owns automatic continuation |
| Cover does not follow album when playback advances | `c5a749b`, `e5bb159` | Player identity and browse-context synchronization |
| Drawer takes too long to open | `e4aa9f8`; SQLite-first album fetch, adjacent prefetch | Cached model data and cancellation for stale async results |
| Starting halfway loses earlier songs for Repeat All | `4400c8f`, tests in `test_playback_modes.py` | Preserve prefix queue without restarting current audio |
| Slow favourite/playlist edits | `cf672a9` release preparation and userdata tests | Optimistic UI with rollback; bulk playlist persistence in one write |
| Incorrect dropdown totals | `d2f9314`, `browse-counts.js` | Server total distinct from page count and grouped cover count |
| Wi-Fi hotspot disappears before connection succeeds | Network worker/handoff fixes | Acknowledge connect asynchronously and restore hotspot on failure |
| Multiple clients trigger competing Wi-Fi scans | `d20a3bb`, cache/lock/background scan | One shared scan, cached reads in settings polling |
| exFAT/local USB mount failures | `b5d3089`, mount fallbacks/packages | Filesystem-appropriate options, do not assume Unix ownership flags work everywhere |
| Radio logos fail cross-origin or block browse | `687c927`, `a744160` | Same-origin proxy plus disk/background caching |
| Touch keyboard hidden behind fullscreen UI | `17de2d0` | Keyboard belongs inside active fullscreen surface |
| OTA broke installed appliance | June conversation, `324c2d9` through `8a038d6` | Transaction, lock, detached service, permissions, health and rollback |
| Obsolete splash causes blanking/startup complexity | `c924671`, `9cb6765`, `cda5a77` | One clear splash/readiness path, not competing splash daemons |
| PC GRUB normal.mod rescue | User physical photo; module existed in built image; recovery UUID search | Do not infer missing file from error alone; firmware disk numbering differs |
| PC LightDM endlessly fails | User photos; `4bccf05` and recovery helpers | Bounded graphics attempts and retrievable report |
| PC 64-bit Chromium crashes in emulation | `e08dc50`; screenshot showed renderer error code 11 | Explicit software rendering stabilized boot, but is not a performance solution |
| Etcher cannot write archive | Download SHA-256 passed; user succeeded with Raspberry Pi Imager | Separate flashing-tool failure from image/boot failure |
| PC boots but cursor is unusably slow | Latest user report on 1 GB 32-bit PC | Test physical responsiveness, not just boot and silent playback |

The early report of songs restarting halfway was not reproduced with a complete hardware/stream trace in the available history. Queue reconstruction and stale UI updates are relevant historical risks, not a proven exclusive root cause for that specific report.

## 6. Failed Approaches and Lessons

- Do not assume Raspberry Pi OS desktop x86 compatibility makes an ARM image cross-bootable.
- Do not solve old-PC boot by changing the working ARM image or Pi boot files.
- Do not equate successful image creation with a usable player. Real 1 GB performance was not part of the original acceptance gate.
- Do not globally force SwiftShader as the final low-memory-PC strategy. It moved rendering to the CPU and solved one emulation crash, while the physical user reported unacceptable lag.
- Do not make local UI depend on network-online, wireless devices or Samba readiness.
- Do not run a file-sharing server simply to read files from a NAS.
- Do not group albums by per-track singer or by only the album title. Both caused important classes of identity errors.
- Do not reset player source or rebuild/clear the queue during ordinary player polls, browse changes, scanning or mode changes.
- Do not queue thousands of tracks synchronously before starting the selected one.
- Do not discard earlier queue tracks when starting in the middle. Repeat All still needs them.
- Do not accept a stale async result after the user switched album/source. Use request generations/identity checks.
- Do not animate with unconditional high-frequency redraws when nothing is moving.
- Do not repeatedly call CoverFlow refit/immediate-layout while a browse animation is active. Existing code has an explicit warning about this regression class.
- Do not treat the legacy MPD title-based routes as equivalent to ID-based SQLite routes for duplicate-title albums.
- Do not claim receiver metadata/arbitration works merely because AirPlay and Bluetooth daemons are installed.
- Do not restart an OTA in the lifetime of an HTTP request. It must continue under a service if the browser disconnects or the API restarts.
- Do not run blind `apt upgrade` or full OS upgrades as App OTA.
- Do not publish A/B system updates because the script exists; hardware power-loss testing is still a separate requirement.
- Do not reuse the original GitHub release channel in PiTunes-Native. An original release can replace platform-specific files.
- Do not respond to a build failure by repeatedly extending timeouts without preserving evidence. The failed runs exposed distinct workflow, renderer, API and test-design defects.
- Do not use removal of the whole QEMU video device as the only display-failure test: that VM did not reach a reachable backend. Force Xorg failure on a disposable copy with known-bootable hardware instead.
- Do not assume `/api/services` is fast or that its top-level `active` exposes `failed`; actual component states must be examined.
- Do not let temporary `/tmp` builds become the only copy of a candidate. A Mac restart cleared the previous temporary test infrastructure and caused repeated rebuilding.
- Do not use force push to address a normal GitHub Compare & pull request banner. Normal pushes/merges worked; the later issue was failed checks, not Git refusing the code.
- Do not market a promise of no surprises or bulletproof updates. Publish precise test scope and remaining risks.

## 7. Raspberry Pi Compatibility

**SOURCE** public target matrix:

| Hardware | Image architecture | Base |
|---|---|---|
| Pi 3, Pi 3 B+, Pi Zero 2 W | `armhf` | Raspberry Pi OS Lite Bookworm |
| Pi 4, Pi 5 | `arm64` | Raspberry Pi OS Lite Bookworm |

Base image URLs are pinned in `image/build.env` to `RPIOS_RELEASE=2024-11-19`. An image label alone does not establish its installed package patch levels; the installer runs package operations. Do not describe this source as using Debian 13 for Raspberry Pi.

**USER-REPORTED**: the Pi edition performs well compared with the old 32-bit PC. This is not a measured model-by-model benchmark. No evidence establishes every advertised HAT, USB DAC and screen on all Pi models.

Pi Zero 2 W memory is particularly constrained. The roadmap mentions a 2D no-WebGL fallback but the current renderer is Three.js; do not mark the fallback complete. Native Widgets can address this, provided memory/paint work is bounded.

Pi audio and boot paths include `/boot/firmware/config.txt` and legacy `/boot/config.txt`, device-tree overlays, Pi KMS/FKMS behavior and `raspi-config`. Pi 5 does not have the same analog-output hardware as older models; route visibility should become capability-based, not just a fixed four-entry menu.

Raspberry Pi `tryboot` support and EEPROM/firmware behavior must be checked per target for any A/B rollout. Existing public two-partition images do not become A/B capable by updating a shell script.

## 8. Intel and AMD PC Compatibility

**SOURCE**:

- `amd64`: Debian trixie, `linux-image-amd64`, BIOS `i386-pc` GRUB plus `x86_64-efi` removable boot.
- `i386`: Debian bookworm, `linux-image-686-pae`, BIOS GRUB plus `i386-efi` removable boot. Intended minimum CPU includes PAE and SSE3 because the browser stack has requirements beyond the kernel.
- 64-bit CPU should generally use amd64 even if Windows was 32-bit. OS bitness is not CPU capability.
- Images are persistent USB system disks: 5 GiB raw size, recommended 8 GB or larger media. They are not installable live ISOs and have no internal-disk installer.
- MBR partition table: first partition FAT32 ESP, 128 MiB, begins sector 2048; second partition ext4 root. BIOS embeds its core in the post-MBR gap.
- UEFI removable boot files are `EFI/BOOT/BOOTX64.EFI` or `BOOTIA32.EFI`. Secure Boot is not supported by this unsigned custom image.
- PC kiosk user is `kiosk`, local autologin, no shell password usable as a public login, no sudo group. SSH is disabled in the PC builder.
- PC image marker `/etc/pitunes-pc-image`; source identity `config/.install-commit`, later also `PITUNES-BUILD.txt` on FAT boot partition.
- `/boot/efi` uses a bounded/nofail fstab option in the current builder; the original preview predates some recovery fixes.
- Pi I2S HAT controls and Pi update methods are hidden/disabled by PC staging patches. USB/class-compliant ALSA devices remain available.

**HISTORY**: the originally inspected amd64 artifact had `/etc/debian_version` 13.8, kernel `6.12.117+deb13-amd64`; i386 had 12.15 and kernel `6.1.0-56-686-pae`. These are observations of specific builds, not pins for all future builds: the PC builder uses current suite/security repositories.

**USER-REPORTED**: first boots reached GRUB rescue, later Linux boot showed repeated LightDM failures, Broadcom firmware warnings and Samba failure. Latest GitHub-built image successfully booted after writing with Raspberry Pi Imager, but was unusably slow. Therefore boot reliability improved without satisfying the performance requirement.

## 9. Intel Atom and Low-Memory Performance

The owner reported a 32-bit PC with 1 GB RAM and approximately minute-scale cursor movement. Exact CPU model, graphics PCI ID, measured CPU usage, swap configuration, USB random-I/O speed and memory-pressure trace were not captured. Do not invent them or claim a confirmed sole cause.

**SOURCE**: current PC staging forces Chromium `--use-gl=angle --use-angle=swiftshader --enable-unsafe-swiftshader` and disables GPU rasterization. SwiftShader means CPU software graphics. Existing WebGL, browser process overhead, library scanning, textures, cache, and limited memory all contribute potential load. Slow USB storage and swapping are plausible additional contributors, not measured conclusions.

**HISTORY**: VM memory measurements around 636 MiB used for an amd64 kiosk were taken with a larger allocated guest and are not a guarantee for a 1 GB Atom. Host/emulation overhead differs from guest metrics. `library/system.py:_memory_usage` labels the fourth whitespace field of `free -h` as available, which is actually the free column on typical output; do not use that field as reliable memory headroom.

Installing to an internal SSD may improve boot, scan and swap I/O but does not eliminate software 3D rendering cost. The existing image has no install-to-disk workflow; the user explicitly expected one and was surprised by persistent USB operation.

antiX was discussed as a possible lightweight 32-bit OS. It is systemd-free, whereas this installer and service-control APIs assume systemd and include Pi-specific operations. Do not claim PiTunes can be installed unchanged on antiX. No antiX integration or performance validation was implemented.

**RECOMMENDATION** for Native:

- Base acceptance on the real 1 GB target, not just 2 GB QEMU guests.
- Use QWidget painting/rasterization with cached images, a small visible cover window, clipped dirty-region updates and no background animation timer when idle.
- Never use Qt WebEngine or a hidden Chromium process as the local display replacement.
- Keep MPD decoding separate; do not decode audio in the Qt GUI thread.
- Load thumbnail-sized artwork asynchronously, bound memory and request counts, and avoid decoding full-size art just to display 128 px.
- Use model/view virtualization for songs and artists. Do not create a QWidget per library item.
- Measure cold boot, time-to-first-input, cursor/touch latency, idle CPU, animation CPU, peak RSS, swap, page faults, scan interference and continuous-playback underruns.
- Make a simple list/grid presentation available on weak machines while preserving library and playback features.
- Set numerical performance budgets after baseline measurements. This handoff does not fabricate a validated frame rate or RAM ceiling.

## 10. Boot Optimization and Startup

Primary sources: `scripts/setup-kiosk-boot.sh`, `setup-kiosk.sh`, `pitunes-kiosk-launch.sh`, `pitunes-display.sh`, `firstboot.sh`, `config/systemd/*`, `systemd/*`, `docs/BOOT_PERFORMANCE.md`.

### 10.1 Actual startup distinctions

`setup-kiosk.sh` configures LightDM to autologin into Openbox and writes Openbox autostart, which launches `pitunes-kiosk-launch.sh`. It disables the older `pitunes-display.service` if installed. That older unit/script still exists and is referenced by other units and documentation. Do not assume both are intended to launch Chromium together, or blindly delete one without reviewing unit dependencies.

The old display service has `User=pi`, `/home/pi`, and UID-1000 session assumptions. PC uses `kiosk`. This is an important reason to create a native display unit with explicit tested user/session ownership rather than repointing the old unit name without analysis.

PC launch readiness patch waits for both `/api/health` and `/api/status` with a 60-second ceiling and 0.25-second poll. The Pi static-first launch behavior differs. `docs/BOOT_PERFORMANCE.md` contains legacy statements about the display service/static readiness; read it alongside actual setup scripts.

### 10.2 Implemented optimizations

- nginx override removes network-online from the critical path.
- NetworkManager-wait-online is disabled by appliance installation; a timeout drop-in also exists.
- MPD starts without waiting for library mount/index completion.
- Startup scans are background oneshots after mount/display-related units.
- Pi Samba is deferred by a 10-second timer rather than blocking display.
- PC recovery disables Samba/nmbd/hotspot/wireless audio auto-start, and replaces music mount unit to depend only on local filesystems.
- X background is dark before the player appears. In-page PiTunes splash is the chosen presentation.
- Firstboot generates machine identity/SSH host keys, handles user configuration, and suppresses tty1 login prompt.
- Screensaver/DPMS are disabled with `xset s off`, `s 0 0`, `dpms 0 0 0`, `-dpms`, `s noblank`; sleep targets are masked in PC build.

### 10.3 Plymouth status

Custom Plymouth assets and old framebuffer splash were removed from source. Pi setup disables/masks Plymouth startup/quit/wait/reboot/poweroff units and masks `pitunes-fb-splash.service`. Package presence on a Debian base is not proof Plymouth is used. Debian LightDM's vendor unit can reference Plymouth failure cleanup even when that service is absent; PC drop-in replaces its OnFailure handler.

### 10.4 Native startup design requirement

Start native shell as an unprivileged service/session, immediately render usable local navigation while backend and library become ready, and show explicit unavailable/loading states. Do not make first paint require NAS or a full library scan. Retain display-awake behavior for the chosen X11/Wayland/DRM platform. No verified implementation currently guarantees a few-second cold boot on arbitrary old BIOS hardware.

## 11. Graphics Drivers, Rendering and Display Recovery

### 11.1 PC recovery implementation

`image/pc/boot/configure.sh` installs:

- `/usr/local/libexec/pitunes-pc/start-display` from `start-display.sh`.
- LightDM systemd drop-in from `lightdm.conf` with preserved runtime directory, bounded start burst and report OnFailure.
- `pitunes-pc-display-report.service` and `display-report` helper.
- PC mount unit independent of network-online.
- BIOS embedded GRUB UUID lookup and compatible graphics menu.

`start-display.sh` first tries Xorg defaults, then fbdev if `/dev/fb0` exists, then VESA. It writes a `90-pitunes-pc-compat.conf` LightDM override and runtime xorg.conf. Marker files in `/run/pitunes-pc-display` prevent endlessly restarting the same mode. `StartLimitBurst=3`, `StartLimitIntervalSec=infinity`, `Restart=on-failure`, `RestartSec=3s` bound service restarts. Verify behavior across a manual stop/start as well as cold boot; persistent override and per-boot marker lifetimes are different.

If all display attempts fail, `display-report.sh` writes `pitunes-display.log` on FAT `/boot/efi` so a Mac/Windows user can retrieve it without SSH. It includes OS/kernel, selected DMI fields, LightDM/Xorg logs and filtered graphics/ACPI/b43 kernel messages. These logs can still disclose host/user/path information; review before public sharing.

GRUB's compatible-graphics menu uses `nomodeset pitunes.safe-graphics=1` and root UUID. It does not disable ACPI. fbdev/VESA are recovery paths, not accelerated performance solutions.

### 11.2 The earlier GRUB failure

Photos showed `normal.mod not found`; read-only image inspection established the module existed. The physical PC had both hd0 and hd1, with two partitions each; an `unknown filesystem` on one partition did not identify the USB. Some typed commands also had punctuation errors. Do not declare the filesystem or Windows disk corrupted based on those outputs.

Recovery core now embeds `search.fs_uuid <root UUID> root`, sets prefix to `($root)/boot/grub`, and includes `biosdisk part_msdos ext2 search_fs_uuid normal`. BIOS and removable UEFI paths are both installed. This improves portability across disk enumeration, but no diagnostic trace conclusively established that disk numbering alone caused every original physical failure.

### 11.3 Firmware warnings

The user's ACPI `AE_ALREADY_EXISTS` / `_OSC` errors came from BIOS tables and appeared before successful Linux filesystem mount. They are not equivalent to failure of the whole OS to boot. Do not set `acpi=off` globally to hide them: it can change power, interrupt and device behavior.

Missing Broadcom `b43/ucode15.fw` establishes unavailable firmware for that Wi-Fi device. It does not prevent local music or explain Xorg failure by itself. Linux Wireless states extracted b43 firmware cannot be redistributed as ordinary bundled distribution firmware; the public image did not include it. An explicit, licensed per-device installation path or supported adapter is required.

### 11.4 Native replacement

Keep a display backend with broad legacy support, but remove WebGL/SwiftShader dependence. QPainter does not itself fix a broken X server; first validate Qt's chosen platform plugin and graphics/input stack. Preserve a headless working backend if the local window cannot start. Native process health should be distinct from display-manager health and API health.

## 12. Audio Architecture: MPD, ALSA, DAC HATs and USB DACs

### 12.1 MPD baseline

`configure-mpd.sh` writes `/etc/mpd.conf`, with music `/mnt/music`, MPD user `mpd`, loopback 6600, database `/var/lib/mpd/tag_cache`, state `/var/lib/mpd/state`, playlists and sticker DB. `auto_update` is off; scans explicitly request MPD updates. `restore_paused` is yes. Audio output type is ALSA. Buffer size is 2048 KiB, prebuffer 10%, maximum playlist 16384 and connections 20. Follow-symlink options are enabled; security boundaries need review.

`backend/mpd_client.py` retries failed commands once after resetting its thread-local socket. Important residual risk: retry of a non-idempotent command after an ambiguous network failure can repeat the operation. Keep transport semantics and test them; do not add retry-all logic casually in Qt as well.

### 12.2 Output discovery and apply

`audio_output.py:list_alsa_devices` parses `aplay -l`, labels devices and returns `plughw:<card>,<device>` values. Card numbers can change after plugging/rebooting; stable ALSA card identifiers would be a future improvement, not an existing guarantee.

Apply goes through `apply_audio_output`, a constrained `sudo systemd-run --wait --pipe --collect --unit=pitunes-audio-apply /bin/bash ...` operation. Supported mixer values are software/hardware/none. Native UI must show pending apply, returned errors and reboot requirement; it should not assume a settings write has already switched hardware.

Pi routes are USB DAC, DAC HAT, HDMI and headphones. `config/dac-hats.json` carries product IDs, labels, overlays, mixer hints and optional config lines. Families include HiFiBerry, IQaudio, Allo, JustBoom and official Pi DAC variants. A catalogue entry is not a physical verification record.

`apply-audio-output.sh` writes/removes marked Pi audio blocks in boot config, disables onboard/KMS audio for some HATs, restores it when switching away and may require reboot. It exports output/mixer environment values into `configure-mpd.sh`.

### 12.3 Important audio code gaps to carry as risks

- **SOURCE / RISK**: base `configure-mpd.sh` uses a quoted heredoc containing literal `${MIXER_TYPE}`; the PC patch substitutes it in `pc-alsa`, but base Pi output generation needs verification. Do not treat software/hardware selection as proven solely by the API response.
- Base Pi script hard-codes USB card 1 / HAT card 0 and does not use every exported `ALSA_DEVICE` / `ALSA_CARD` value. The frontend exposes a device choice more general than the base script fully implements.
- `restore_onboard_audio` contains a suspicious mismatch between the KMS flag it detects and the substitution it restores. Test round trips HAT -> HDMI/headphones on actual Pi hardware.
- Existing `detect_usb_card` heuristics and labels are not a substitute for ALSA capability checks.
- Two separate audio receivers and MPD can compete for ALSA default; missing source arbitration must be addressed deliberately.

### 12.4 PC-specific path

The PC patch adds `pc-alsa`, accepts only `default` or numeric `hw:`/`plughw:` addresses, validates mixer values, substitutes the mixer and explicitly writes device selection. It hides HAT route/catalogue and prevents Pi boot overlay operations on x86. This behavior is in the staging patch, not the unpatched Pi script.

USB class-compliant DACs use existing Linux USB audio support. Vendor-specific devices and unusual sample formats need actual testing. Pi I2S HAT support does not transfer to PC GPIO-less hardware.

## 13. Audio Quality and Playback Correctness

**USER REQUIREMENT**: UI migration must not degrade playback quality, restart tracks, change formats, force resampling, change volume behavior unexpectedly, or monopolize CPU while audio plays.

No audited bit-perfect-output matrix, USB-DAC sample-rate capture, distortion/noise measurement or cross-device underrun test was found. `plughw` can perform ALSA format conversion, software mixing alters samples, and AirPlay/Bluetooth protocols have their own constraints. Do not advertise bit-perfect or lossless transport across all routes.

Retain MPD decoding and hardware configuration during initial Qt migration. Do not add a PulseAudio/PipeWire/Qt Multimedia audio path without a deliberate tested design, especially when MPD, Shairport and BlueALSA already target ALSA.

### 13.1 Fast queues and continuation

`play_queue_fast` starts the selected track with `clear -> add -> play 0`, appends subsequent tracks asynchronously, then prepends tracks preceding the selected track using `addid ... 0` without restarting current playback. `SYNC_QUEUE_HEAD=0`. Cancellation generations ensure an older worker stops when a new queue takes over. This must survive native multi-clicks and source changes.

`maybe_continue_next_album` is guarded by a lock and interval. It checks MPD stopped/end-of-queue state and shared browse context, then appends the next album. MPD `idle player playlist` watcher and a 1.5-second backup poller provide UI-independent behavior. Native clients must not add competing automatic-next-album logic.

Repeat modes map to MPD `repeat`/`single`; shuffle maps to `random`; repeat modes set `consume 0`. Backend continuation is suppressed when repeat or random is on. UI must reject these options for radio/external sources and restore state on failed changes. Tests cover starting mid-album, retaining earlier tracks, cancellation and stale polls.

### 13.2 Playback bugs to test again

- Long track runs beyond the halfway point, including seek/pause/resume.
- Full queue retained when starting middle/last track; no automatic rewind on refresh or scan.
- Manual Next skips in Repeat Song while automatic end repeats it.
- Source unavailable after unplug/NAS loss is an explicit error, not a corrupt queue reset loop.
- No double advance from both idle event and backup poller.
- No queue restart during album artwork or metadata refresh.
- Native close/restart and loss of HTTP clients do not stop local MPD playback.

## 14. Bluetooth and AirPlay

**SOURCE** receiver setup in `scripts/setup-wireless-audio.sh`:

- BlueZ main configuration sets appliance name, audio class, discoverability/pairability behavior.
- BlueALSA is configured as A2DP sink, not a generic Bluetooth output-device manager.
- `pitunes-bluealsa-aplay.service` forwards A2DP input to ALSA default; companion pairing agent uses `bt-agent`, and discoverability helper uses Bluetooth tools.
- Pairing agent stop timeout was reduced to avoid shutdown hang (`3c985a2`).
- Shairport Sync uses ALSA `default`, Avahi mDNS, basic interpolation and metadata disabled in generated config.
- `nqptp` is optional at install time. Package support and platform determine AirPlay protocol features; do not infer capabilities from service name.
- Settings service control starts/stops/enables/disables related helper units, not only one daemon.

**RISK / UNRESOLVED**:

- Optional `input_sources` is missing. Auto-pausing MPD, input takeover, receiver metadata/artwork synchronization and transport forwarding are not fully wired in tracked source.
- The browser has external-source UI and placeholder art routes, but UI support does not supply daemon metadata.
- No paired-device list / forget-device management API was identified. Roadmap lists this as pending.
- Permanently discoverable/NoInputNoOutput pairing is a usability-security tradeoff; native UI needs clear optional enablement and state.
- On recovery PC builds these receivers default off and must not block boot without hardware.

A future source manager should own local/radio/AirPlay/Bluetooth state and audio-device arbitration, expose a consistent API, and be tested against actual receiver events. Do not invent that manager inside QPainter rendering code.

## 15. Wi-Fi, Hotspot, Network and NAS

### 15.1 NetworkManager ownership

`network_wifi.py`, `scripts/setup-wifi.sh`, `wifi-hotspot.sh` and NetworkManager own Ethernet/Wi-Fi/AP. Standalone hostapd, dnsmasq and dhcpcd services are disabled by installation to avoid multiple managers owning an interface. dnsmasq-base is a dependency for shared AP operation, not a second independently managed DHCP stack.

`wifi_status` returns station/AP/interface/default route and Ethernet-related state. Scan supports a cached-only query, lock and a shared background scan to avoid repeated expensive radio scans from multiple settings clients. Connection is a worker operation with a persisted status response; the API acknowledges before AP teardown. Failed join can restore the hotspot.

Default hotspot address is `172.24.1.1`, appliance SSID is PiTunes. Do not place the shipped shared hotspot password in this document; change setup credentials appropriately before distribution. Credentials are sensitive even when an old README printed a default.

PC has `net.ifnames=0` but not every old Wi-Fi adapter necessarily becomes `wlan0`. Some helpers still assume conventional interface names; verify adapter/interface discovery before claiming broad PC support.

### 15.2 NAS is a client feature

`storage_sources.py:network_storage_configure` accepts protocol smb/nfs, server, share, username and password, writes mode-0600 config, restarts the music mount service and verifies resulting filesystem type. Status deliberately omits the stored password. SMB and NFS mounts are read-only for playback.

Source settings are switched to network before attempting mount; failure can leave network selected. Native UI must preserve error context and let the user return to local source without reboot.

The shell helper pings the server up to 12 times with five-second sleeps, then retries filesystem mounts. Hosts blocking ICMP can be reachable for SMB but rejected by this readiness approach. NAS mount timeout can outlast API/nginx timeout. These are integration risks; never block the Qt event loop waiting for them.

### 15.3 Samba server is different

Pi Samba shares `/mnt/music` with guest-write configuration using MPD/audio ownership. PC recovery turns it off and disables legacy NetBIOS (`nmbd`). This does not stop mounting someone else's NAS. No general Samba-sharing toggle exists in the current UI service map; don't claim it can be enabled through Settings without adding a proper API.

### 15.4 Offline requirement

MPD, local API, native display and USB indexing must run without a NIC. NetworkManager can remain installed for later Ethernet/Wi-Fi configuration, but no network-online dependency should gate the core. Radio/NAS unavailable offline is normal; local player failure because they are unavailable is a regression.

## 16. USB Storage and Music Library Mounting

`scripts/mount-music-drive.sh` reads `storage_source` and `local_device`. Sources:

- `local`: prefer explicitly selected device, then partitions labelled MUSIC (case variants), then USB block devices/partitions discovered with lsblk.
- `internal`: bind `/var/lib/pitunes/music` to `/mnt/music`.
- `network`: mount stored SMB/NFS config at `/mnt/music`.

Local mounts first try uid/gid/umask options and fall back to generic Unix-filesystem options. FAT/exFAT/NTFS support packages are installed. The PC patch skips its own boot/root partitions during music-device enumeration to avoid scanning/mounting the system disk as music.

After finding supported audio, the helper posts `/api/library/rescan`. Udev rule `config/99-pitunes-music.rules` requests `pitunes-storage-refresh.service` on partition add/change. Review all related service ordering for mount churn under repeated hotplug events.

`filesystem_roots` distinguishes USB local drive, Internal Storage and Network Storage. Folder browse lists up to 300 directories and uses settings to choose the scan directory. `music_directory` and MPD's fixed `/mnt/music` are not identical concepts; test selecting a subfolder and resulting URI resolution before native migration claims arbitrary-folder support.

**RISKS**:

- Selected local device is stored as a device path, which may change after reboot/hotplug.
- API reset detection compares source/directory but not every case of swapping a different drive at the same mount path.
- Mount helper may unmount current source; do not trigger remount just because a settings view refreshed.
- Lost mount during scan can make stale cleanup destructive to the index; retain user data and handle missing source separately from a truly empty library.
- Do not run recursive chown on user media as generic troubleshooting; especially not mounted NAS/system disks.
- Internal Windows disks must never be formatted, repartitioned or automatically installed over. Existing USB image has no such installer.

## 17. SQLite Database Architecture and Schema

**SOURCE** `backend/library/db.py`, `SCHEMA_VERSION=5`. Thread-local connection, row factory, WAL, synchronous NORMAL, foreign keys ON, temp_store MEMORY, initialization lock. Do not have Qt write this DB directly while Python owns it.

### 17.1 Tables

| Table | Columns and semantics |
|---|---|
| `artists` | `id INTEGER PRIMARY KEY`, `name TEXT NOT NULL COLLATE NOCASE UNIQUE`, `sort_name TEXT NOT NULL DEFAULT ''` |
| `albums` | `id PK`, `title TEXT NOT NULL COLLATE NOCASE`, `artist_id REFERENCES artists`, `album_artist_id INTEGER`, `year INTEGER`, `genre TEXT`, `rating INTEGER DEFAULT 0`, `track_count INTEGER DEFAULT 0`, `duration_sec REAL DEFAULT 0`, `art_path TEXT`, `art_mtime INTEGER`, `updated_at INTEGER DEFAULT 0` |
| `tracks` | `id PK`, `album_id NOT NULL REFERENCES albums ON DELETE CASCADE`, `file_path TEXT NOT NULL UNIQUE`, `title`, `artist`, `composer` strings, `track_number`, `disc_number DEFAULT 1`, `duration_sec DEFAULT 0`, `rating DEFAULT 0`, `mtime INTEGER NOT NULL`, `file_size INTEGER NOT NULL` |
| `scan_runs` | `id PK`, start/finish timestamps, seen/added/removed/updated counts, status string |
| `app_settings` | `key TEXT PRIMARY KEY`, string `value`, `updated_at` |
| `search_fts` | FTS5 virtual table: album_title, artist_name, track_title, album_id UNINDEXED; unicode61 with remove_diacritics=2 |

Indexes cover albums artist/title/year and tracks album/path. `album_artist_id` is an integer without a declared FK in the schema. Album identity is enforced by application lookup, not a declared composite unique constraint. Do not infer stronger database guarantees.

Migrations before v3 add ratings; before v4 track artist; before v5 composer. Migration exception handling is broad. Preserve data and test supported upgrade paths rather than replacing schema blindly.

### 17.2 Identity contract

- HTTP album ID is a stringified integer, not album title, artwork URL or row position.
- Track public ID is generally the relative file URI/path, not tracks.id.
- Album matching: title case-insensitively + resolved album artist ID + year including NULL semantics.
- UI `album-identity.js` adds NFKC/whitespace normalization and authoritative-ID preference; year and album artist qualify fallback matching.
- Album favourite IDs are numeric database IDs as strings. A destructive rebuild can change identity and favourites association; design a migration/reconciliation strategy before changing it.

### 17.3 Other stores

`userdata.json` has favouriteTracks, favouriteAlbums, playlists, radioStations. Playlist IDs/station IDs are UUID-like hex strings. Writes use a process lock plus temp-file rename; readers are not uniformly locked. `ui-context.json` uses a lock and revision but writes directly rather than an atomic replace.

`load_settings()` reads JSON and overlays SQLite app_settings, whose values are strings. JSON vs SQLite type conversion and authority can differ. Native should consume normalized API settings and test booleans/numbers, not assume both stores contain identical typed JSON.

## 18. Library Scanning, Metadata and Artwork

### 18.1 Incremental scan

`scanner.py` maintains ScanState lock, worker, generation, progress count, last errors and scan_runs. It ignores hidden/macOS/resource/recycle/NAS metadata directories, parses tags with Mutagen, uses Python wave duration fallback, compares mtime/size and metadata/album assignment, updates rows and deletes stale paths, recomputes album stats and rebuilds FTS.

Progress is a count of files seen, not a validated percentage of a known total. Update native progress presentation accordingly. Mark unfinished scans interrupted at server startup. A new forced generation cancels prior work and waits for prior thread completion before resetting/rebuilding.

The scanner reparses metadata on traversal; 'incremental' does not mean every unchanged file avoids all metadata I/O. It builds an in-memory existing-file map and seen set; very large libraries still have memory cost. It commits at album flush intervals and flushes groups around 25 files for early visibility. Playback activity causes a 0.25-second yield window for approximately 20 seconds.

### 18.2 Album and artist rules

Tags include title, album, track artist/singer/performer, album artist, composer, year/date, genre, track/disc and duration. Missing title falls back to filename. Missing album uses cleaned folder name. Album artist uses explicit album-artist metadata where present, otherwise common artist for a single-artist group or Various Artists for compilations.

Grouping within folders uses tagged album, explicit album artist and year. `_folder_album_metadata` and `_get_or_create_album` are important. Mixed metadata, very large partially flushed groups, same-title reissues and identical titles across years require regression fixtures. Do not simplify to one album per file or one album per directory without preserving multi-album folders.

### 18.3 Artwork resolution

`art_resolver.py` supports folder artwork and embedded art, including Mutagen extraction plus dedicated FLAC/MP4/ID3/RIFF handling. Thumbnail endpoints offer 128 px for requested sizes <=160, otherwise 420 px. Pillow is optional at import time; generated virtual covers and format fallback need testing if absent.

`get_art_file` first checks numeric album/revision cache, resolves album metadata/source, may return folder art, then embedded art, finally a virtual album/title cover. A meaningful discrepancy: settings/docs say embedded-first but this route explicitly asks for folder art with `prefer_folder=True` before its final embedded extraction. Record behavior rather than promising universal embedded-first precedence.

Queries attach `artUrl` with album ID and library revision. Frontend additionally caches textures and resolves radio/external placeholders. Wrong art historically came from identity/cache coupling; native must key cache by source identity + album/file + revision + target size, not only title.

Radio icons are proxied through the backend, use a 7-day disk TTL and background prefetch. Saved station list avoids synchronous enrichment. Unknown radio logos use a specific boombox/white placeholder asset, not a random album cover.

`rebuild_art_cache` removes cache, invalidates album art fields/revision and starts scan work; it is not a harmless repaint. Do not run it during every native startup.

### 18.4 Query/performance details

- Album queries use LIMIT/OFFSET; API default 96, web frontend page size 200.
- Full-song default limit 10000; web songs browse initial cap 5000.
- Search uses FTS5 and fallback paths; it is not unbounded all-field metadata search.
- Missing duration can be repaired on query using local probes, then committed. A browse read can therefore have I/O and DB side effects.
- SQLite-first song drawer and adjacent prefetch avoid repeatedly asking MPD for each album.
- `list_starred_tracks` handles larger input sets in tests, and playlist presentation reconstructs stored order rather than relying on query order.

## 19. Complete Backend HTTP API Reference

This is the route inventory from `Handler.do_GET`, `do_POST`, `compat_player_post`, `POST_ACTIONS` and `SIMPLE_MPD_POSTS` at the baseline. It is not a generated OpenAPI contract. No PUT/PATCH/DELETE API was implemented in this dispatcher. Unknown routes return 404; unsupported verbs follow BaseHTTPRequestHandler behavior.

### 19.1 Transport and data conventions

- JSON request bodies with Content-Length; empty body becomes `{}`. Invalid JSON returns `{"error": ...}` with 400. Most exceptions return 500 and their string; MPD failures commonly become 502.
- Application API has no per-user authentication. Intended for trusted LAN only; nginx exposes it on port 80.
- Many successful operations return 200 even if payload has `ok:false` or an `error` field. Native must inspect body, not only HTTP code.
- Query parameters are parsed with `parse_qs`; repeated values often use first. Numeric validation/bounds are inconsistent across routes; invalid integers may become 500.
- Names/IDs differ by endpoint. Track ID usually equals relative file path. Numeric album ID is preferred over legacy album name.
- JSON naming mixes camelCase/snake_case and MPD Title/Artist/Album fields. Normalize in a client adapter.
- No event stream/WebSocket/SSE exists. Web UI polls; MPD idle is internal to backend.
- `/api/health` only reports handler readiness/time/library availability. It is not a complete audio/display/hardware self-test.
- Some routes call slow system commands; asynchronous Qt requests must have operation-specific timeouts and cancellation generations.

### 19.2 Library and artwork GET routes

| GET path | Query / response / notes |
|---|---|
| `/api/library/albums` | offset=0, limit=96, filter, sort. Returns albums,total,offset,limit. Filters `artist:`, `composer:`, `year:`, `genre:`, favourite-prefix, toprated/highest/rating; sort title,artist,year-asc,year/year-desc,rating aliases |
| `/api/library/album/{albumId}/tracks` | Numeric ID preferred; legacy encoded-name fallback. Returns tracks; order disc,track,title |
| `/api/library/artists` | artists:[{name,album_count}] |
| `/api/library/composers` | composers:[{name,album_count}] |
| `/api/library/genres` | genres:[{name,album_count}] |
| `/api/library/years` | years:[{year,album_count}], descending |
| `/api/library/tracks` | offset=0,limit=10000. tracks,total,offset,limit when indexed; absent library returns empty |
| `/api/library/favourites` | tracks:[relative-path IDs], albums:[string album IDs] |
| `/api/library/starred/tracks` | Hydrated favourite track entries |
| `/api/library/playlists` | playlists:[{id,name,trackIds}] |
| `/api/library/playlists/{playlistId}/tracks` | playlist object plus hydrated tracks; use playlist.trackIds to reconstruct order; missing ID 404 |
| `/api/library/scan-status` | running,progress,message,lastFinishedAt,lastError,generation,lastRun,albumCount,trackCount |
| `/api/search` | q,limit=120; albums,tracks,total. Empty q gives albums; result total may count returned matches rather than global count |
| `/api/art` | album_id,album,file,size,rev,renderer. Returns image/SVG; size tier 128/420. renderer airplay/bluetooth gives generic source artwork |
| `/api/stream` | file=relative URI; byte streaming, MIME, Accept-Ranges; valid single-range requests get 206, invalid range 416 |
| `/api/albums` | Legacy MPD album-name list and art_url; cannot fully disambiguate duplicate titles |
| `/api/artists` | Legacy MPD artist-name strings |
| `/api/tracks` | Legacy album or artist query; with no filter and library ready can use indexed pagination; otherwise MPD result |

Album object normally has id,title,artist,albumArtist,year,genre,rating,artUrl. Track object differs by route: file/id,title,trackNumber,duration,artist,singer,albumArtist,year,genre,albumId,artUrl,rating; album-track response also has composer but omits some fields from full-track response. Do not require every property on every route.

### 19.3 Radio GET routes

| GET path | Contract |
|---|---|
| `/api/library/radio` | scope=all/favourites; stations array, favourite-first sorting |
| `/api/library/radio/search` | q,country,tag,limit=20,offset=0; stations,offset,limit,hasMore; failure can return error with empty stations and 200 |
| `/api/library/radio/icon` | url OR stationId, optional title; proxy/cache favicon through `get_radio_icon_file`; returns cached icon or generated fallback |

Station fields include id,name,url/streamUrl,artUrl/favicon,country,language,genre/tags,codec,bitrate,externalUuid/source,favourite as available. Search limits backend-clamped to 1..80. First-page query merges station-name/language/tag/bytag matches; subsequent pages primarily search by name. `hasMore` is heuristic, not a guaranteed total.

### 19.4 Player and shared state GET routes

| GET path | Contract |
|---|---|
| `/api/status` | Flat MPD-style normalized state: state,volume,elapsed,duration,song,playlistlength,playlistposition,updating_db,repeat,random,single |
| `/api/player/state` | inputSource,status nested,song with Title/Artist/Album/file/Time/albumId,uiContext; radio adds radio station payload; external hook conditional |
| `/api/ui/context` | uiContext object with monotonic-ish revision and browse/playback fields |
| `/api/health` | ok,time,library |

Volume -1/no mixer and unknown duration need UI handling. Do not derive native current-track identity from display text. Use file/albumId and canonical queue context.

### 19.5 Settings, hardware and system GET routes

| GET path | Contract |
|---|---|
| `/api/settings` | config.musicDir,config.ui,settings,scan,counts,outputs,libraryBackend; outputs currently empty, use audio/devices |
| `/api/audio/devices` | devices:[{alsa,label,card,device}],hats,routes,current:{route,device,mixer,dac_hat}; PC hides hats |
| `/api/services` | services map ssh,bluetooth,airplay,kiosk, each value an ARRAY of status objects, with components |
| `/api/network/wifi/status` | Ethernet/Wi-Fi/hotspot/connect state; platform tool fallback may return unknown |
| `/api/network/wifi/scan` | cached=1/true/yes for cached-only; networks plus status/cache/error info |
| `/api/storage/network/status` | configured,mounted,mountDetails,protocol,server,share,username,mountPoint; never password |
| `/api/filesystem/roots` | roots with kind,label,path,available,readable,selected,device/action/status where relevant |
| `/api/filesystem/browse` | path; returns path,parent,entries:[{name,path,readable}], capped at 300 directories |
| `/api/system/info` | hostname,uptime,URLs/IP,rootDisk,memory,OS/kernel/architecture/board/temp/Python/API version,PiTunes version/commit |
| `/api/system/update/status` | support/availability/applying/state/version/updateType/message fields; schema varies by state |
| `/api/system/update/log` | lines default 160; log,path,lines,error as applicable |

`service_state` aggregates component failures as top-level active=inactive, not failed. Read components to see actual failed LightDM. This distinction caused a CI test bug. Services enumeration makes many systemctl subprocesses and exceeded short timeouts under emulation.

### 19.6 Library, user data and radio POST routes

| POST path | Body / behavior |
|---|---|
| `/api/library/rescan` | Empty; selected source must be available; starts SQLite/MPD scan |
| `/api/rescan` | Alias routed to library scan, not merely MPD update |
| `/api/library/rebuild-cache` | Empty; resets artwork/cache state and rescans |
| `/api/library/favourites` | trackId/id/file OR albumId, starred (default true); track branch wins if both present |
| `/api/library/playlists` | name required, optional trackIds array and trackId/file; bulk create in one write |
| `/api/library/playlists/tracks` | playlistId and trackId/file; append if not already present |
| `/api/library/radio/stations` | name + URL (url/streamUrl), optional directory metadata/favourite; deduplicates externalUuid |
| `/api/library/radio/favourites` | stationId/id, starred/favourite default true; missing station 404 |
| `/api/library/radio/remove` | stationId/id; removes stored station |
| `/api/ui/context` | Partial supported context fields; returns merged uiContext with updated revision |

There is no existing generic remove-track-from-playlist, playlist rename/delete/reorder, library-tag edit, star-rating setter or smart-playlist CRUD endpoint. Implementing native UI for these requires an intentional API extension.

### 19.7 Playback POST routes

| POST path | Body / behavior |
|---|---|
| `/api/player/play` | file/trackId optional; with file accepts album/albumId,queue URI array,entryId,continuous. Without file sends MPD pause 0 |
| `/api/player/pause` | Empty; pause and clear backend continuous flag |
| `/api/player/stop` | Empty; stop, cancel append worker, clear continuous flag |
| `/api/player/previous` | Empty; previous, clear continuous flag |
| `/api/player/next` | Empty; next, clear continuous flag |
| `/api/player/options` | repeatMode off/all/song and/or shuffle BOOLEAN. Invalid body 400, radio/external input 409 |
| `/api/player/seek` | seconds, clamped nonnegative via seek helper; optional external hook |
| `/api/player/volume` | volume, clamped 0..100; optional external hook |
| `/api/player/queue` | albumId,clear,play; name-based MPD findadd after ID -> title resolution, so duplicate-title ambiguity remains |
| `/api/player/radio/play` | stationId/id OR url/streamUrl and optional name; clear/add/play MPD station |
| `/api/play-album` | Legacy album required; can resolve numeric ID via helper; returns flat status |
| `/api/play-track` | Legacy file plus optional album/albumId,queue,entryId,continuous |
| `/api/pause` | pause 1, cancels legacy append path |
| `/api/resume` | pause 0 |
| `/api/toggle` | MPD pause toggle |
| `/api/stop` | stop, cancels legacy append path |
| `/api/next` | next, cancels legacy append path |
| `/api/previous` | previous, cancels legacy append path |
| `/api/volume` | Legacy volume 0..100 |
| `/api/seek` | Legacy seconds |

Prefer `/api/player/*` and canonical ID/file data for new clients. Legacy paths and optional fallback implementations do not have identical continuation/cancellation semantics.

### 19.8 Configuration and privileged POST routes

| POST path | Body / behavior |
|---|---|
| `/api/settings` | Whitelisted music_directory,storage_source,local_device,audio_output,dac_hat,alsa_device,mixer,animationSpeed,visibleCoverCount,themeAccent. Source changes can remount and scan; response settings/message |
| `/api/audio/output` | output/audio_output,dac_hat,alsa/alsa_device,mixer; apply script then persist; response ok/message/route/reboot_required |
| `/api/network/wifi/connect` | ssid,password or psk,country (default GB); worker acknowledgment then async state changes |
| `/api/network/hotspot/start` | Empty; explicit AP start |
| `/api/network/hotspot/stop` | Empty; stop AP |
| `/api/storage/network/configure` | protocol smb/nfs,server,share,username,password; persist/mount/start scan |
| `/api/services/control` | service ssh/bluetooth/airplay/kiosk; action start/stop; persists enable/disable and controls helpers |
| `/api/system/update/check` | Empty; resolve latest release and supported route |
| `/api/system/update/apply` | Empty; launch supported detached update job; inspect ok/message/status |
| `/api/system/control` | action reboot/shutdown; responds, then daemon thread delays and invokes privileged power command |

Suggested native contract fixtures: empty library, one USB track, compilation album, same-title reissues, no NIC, absent ALSA devices, failed NAS, missing MPD, disabled receivers, update-in-progress, invalid JSON, slow services response, unknown endpoint, encoded Unicode path and byte-range streaming.

## 20. Frontend Functionality, Visual Design and Interactions

Primary source is the approximately 9,000-line `frontend/assets/app.js`; helper modules hold narrower contracts. Do not translate it line-for-line into one enormous C++ widget.

### 20.1 Visual identity

- Dark/black fullscreen player, prominent square center cover with angled adjacent covers, reflected lower artwork.
- Transport strip and album info anchored to measured cover geometry, not independent absolute magic coordinates.
- Title/track artist/album-year readable without occluding controls. Long album/year lines must wrap/fit while keeping year visible.
- Compact dropdowns, counts in headers separated by a vertical bar, no extra descriptive rows.
- Active browse-menu underline; consistent surface and selected-state behavior.
- Repeat/shuffle icons use gray off / white on and no decorative circular backgrounds.
- Existing logo, splash, favicon and radio placeholder should be carried forward with their proper contexts.

### 20.2 CoverFlow constraints

`renderer.js` uses virtual slide cards and a bounded visible-side buffer, textures and reflection masks. Important constants: 304-unit square plane, 52-degree side angle, move 0.42 s, rotation 0.28 s, scale 0.32 s, power2.out easing. The source explicitly warns not to change durations, tween targets, snap/refit behavior casually. QPainter will not reuse world-space units literally, but preserve the perceptual interaction by reference recordings and side-by-side testing.

Separate browsing position from playing identity. A user can browse elsewhere without interrupting audio; after inactivity the UI may snap back to playing entry (historically 5 seconds). Remote UI context and new-track events can recenter correctly across album/song/radio modes. Avoid jumping every time an elapsed-time poll arrives.

Mouse wheel, touch drag/swipe, scrollbar/strip, previous/next covers, tap/release to open drawer, keyboard left/right/Enter/Escape, fullscreen and pointer visibility all matter. Dropdown/dialog interactions must suppress unintended cover drag/click-through. Touch should not be treated as mouse hover.

### 20.3 UI data and loading

Web albums load in pages of 200, songs initially 5000, radio search pages of 20 with 180 ms search debounce. Drawer prefetch surrounds focused album, caches tracks and invalidates when sources/library change. UI context synchronization uses shared backend revision plus local persistence.

Web player refresh is 1.5 seconds; elapsed presentation refresh is 0.5 seconds. A native client can use an event/timer model but must not spin, poll service status at animation rate or recreate artwork per time update.

### 20.4 PC rendering difference

Base Pi renderer still has its original animation frame path. PC build additionally applies `render-on-demand.patch`, which schedules only changed/moving scene frames. It is not a universal source-wide renderer optimization, and it did not make 1 GB PC Chromium acceptable.

## 21. Settings, Favourites, Playlists, Smart Playlists and CSV Export

### 21.1 Settings and persistence

Backend JSON settings and SQLite app_settings coexist. UI preferences include visible covers, animation speed/accent, audio route, music directory, source/device selection; album-info font scale is browser-only. PC routes are dynamically provided by patched API, not hard-coded Pi menus.

Browser storage keys to migrate deliberately:

- `pitunes-smart-playlists`
- `pitunes-output-route` (per-browser playback route, including browser output)
- `pitunes-music-folder`
- `pitunes-browse-state`
- `pitunes-browser-repeat-mode`
- `pitunes-browser-shuffle`
- `pitunes-album-info-font-scale`
- `pitunes-browse-sort-default-year-desc-v1` migration marker

Native should not read a Chromium profile's localStorage files live. Supply an explicit export/import or one-time migration route. Separate per-client view preferences from appliance source/audio state.

### 21.2 Favourites

Song favourites use relative file IDs; album favourites use string album IDs. Frontend immediately updates hearts, guards duplicate in-flight changes and restores prior state if save fails. Favourite albums include all their tracks when exported, not only individually starred songs.

### 21.3 Regular playlists

Backend JSON stores `{id,name,trackIds}`. Bulk create deduplicates IDs while preserving first-seen order and performs one persistent write. Add-track avoids duplicate entries. UI reconstructs stored playlist positions from trackIds. No database table and no full playlist editing REST API exist yet.

### 21.4 Smart playlists

Rules and evaluation are frontend-only in `createDefaultSmartPlaylist`, `applySmartPlaylist`, `smartPlaylistMatchesTrack`, `smartRuleMatchesTrack`, `getSmartTrackValue`, `sortSmartTracks`. Supports any/all matching, field/operator choices, limits and sort options. Operators include is/is-not/contains/does-not-contain/greater-than/less-than/in-range.

The builder exposes many fields (artist, album artist, title, composer, year, genre, rating, duration, bitrate, BPM, plays, dates, etc.) but the backend schema does not populate all of them. Missing/default-derived values mean an advertised rule field is not necessarily meaningful. Preserve working rules and explicitly mark unsupported fields in native rather than invent data.

### 21.5 CSV contract

`music-export.js` collects selected favourite songs, favourite albums, all/selected saved playlists with concurrency at most four. Deduplicates by file/ID, retains memberships and positions, checks cancellation, reports progress and exports without altering library or playlists.

Headers in order:

```text
keep, notes, title, artist, album_artist, album, year, genre,
composer, track_number, duration, format, bitrate_kbps, file_name,
file_extension, relative_path, favourite_song, favourite_album,
favourite_albums, playlists, playlist_positions, selection_sources,
pitunes_song_id
```

Output is UTF-8 with BOM, CRLF rows, every cell quoted, internal quotes doubled. Strings beginning with spreadsheet formula markers after whitespace are prefixed to avoid formula execution. Unknown metadata is left blank; do not fabricate bitrate when schema doesn't provide it. Filename/relative-path columns and blank Keep/Notes are deliberate curation features.

## 22. OTA Updates, Rollback and Recovery Architecture

### 22.1 Three maintenance categories, one UI action

The user wanted clear boundaries:

1. Application changes -> App OTA.
2. Tested OS/security changes -> curated rollback-capable system OTA, only where capable.
3. Major OS/layout changes -> new tested image.

Implementation presents a unified Software Update action; it is not three independent always-enabled update buttons. Descriptor `pitunes-release.json` selects app/system/image. Missing descriptor defaults to app for backward compatibility; invalid descriptor must not silently downgrade to app.

### 22.2 App OTA

`backend/library/system.py` checks latest non-draft/non-prerelease original GitHub release, validates semantic version against config, resolves commit identity and starts `pitunes-update.service`. `scripts/pitunes-update.sh` owns the actual transaction independently of the HTTP request.

Sequence: acquire exclusive lock, record status/log, download immutable source, validate required files/version/syntax, restore executable permissions, back up installed app and owned system files, install scoped files, reload/restart relevant services, poll `/api/health`, mark success/prune backups or restore archives on failure. Retains newest three backup sets by default.

App OTA must not run apt or replace kernel, OS firmware or partition layout. UI overlay blocks interaction during applying state and shows do-not-power-off/progress/error status. Logs and last status persist for diagnostics. It is not a complete power-loss-atomic filesystem transaction; health success is not proof every feature works.

Historical fixes include systemd execution, lock ownership/stale-lock handling, backup-list handling, archive executable bits and post-update rollback. Do not bypass these to simplify native launch.

### 22.3 Signed A/B system OTA

Implemented behind `/etc/pitunes/system-update.json` capability gate. Files: `pitunes-system-update.sh`, `pitunes-system-boot-check.sh`, `build-ab-image.sh`, `build-system-update-bundle.sh`, example capability config.

Layout: stable boot-control partition, Boot A/Root A, Boot B/Root B, shared persistent data. Signed manifest specifies schema/product/version/architecture/strategy plus compressed image asset names, byte sizes and SHA-256. OpenSSL verifies detached manifest signature against installed public key. Private signing key stays offline.

Updater refuses wrong product/arch/strategy/signature, mounted/active-slot writes and incompatible layouts; writes inactive root then boot, patches fstab/cmdline, records pending state, atomically adjusts autoboot selector and requests Pi tryboot. Boot-check tests nginx/API/MPD and health, commits only healthy trial or returns previous default. Missing/invalid pending/control/persistence state has rollback guards tested in shell fixtures.

**NOT production-approved** for broad rollout: power loss during all phases, bad kernel, hardware watchdog and repeated A/B cycles are still mandatory physical gates. Existing public two-partition Pi images stay App OTA. This is Pi tryboot, not PC GRUB A/B support.

### 22.4 Native update boundary

PC patches disable original App/System OTA and related units because original releases would overwrite PC differences. Native must have its own product/repository/version/capability policy and package install/rollback for the Qt binary and dependencies. Do not enable original PiTunes OTA in the new app simply because the API exposes it.

## 23. Security Requirements and Review Findings

This section distinguishes source-visible risk from formally confirmed vulnerabilities. No independent completed penetration test or complete CVE audit was found.

### 23.1 Existing protections

- API and MPD bind loopback by default; nginx is the intended LAN surface.
- Service runs as `pitunes`, not root, with systemd protections and scoped sudo helper permissions.
- Streaming path uses resolved-path containment under music root; rejects traversal and unsupported file suffixes.
- Output apply accepts constrained service/action/route/mixer choices in key paths.
- NAS status does not return password, config/credential files are mode 0600.
- Public images do not ship a known SSH login password; firstboot regenerates unique host keys/machine identity.
- PC kiosk user has no sudo group; SSH defaults disabled.
- Signed system-bundle verification and inactive-slot restrictions protect that gated update path.
- CSV formula escape protects exported values.
- Release tooling checks for private PEMs and obsolete build content.

### 23.2 Risks not to transfer uncritically

- Trusted-LAN unauthenticated API can change Wi-Fi/storage/audio/services, start updates, reboot/shutdown. Never expose port 80 to Internet. Native GUI does not fix remote exposure.
- No documented per-client authorization, TLS, CSRF/origin enforcement or general rate limiting in HTTP dispatcher. Review browser-to-LAN attacks before adding remote access.
- `filesystem_browse` is not confined to music directories; arbitrary readable directory paths can reveal filesystem structure. Settings music path changes are also privileged in effect.
- `post_json` reads declared Content-Length with no clear application size ceiling. Query limits, worker concurrency and external image size need bounds.
- Radio and favicon fetches accept remote HTTP(S) URLs without a demonstrated internal-address/redirect policy. SSRF/private-network access and oversized/decompression-heavy media are risks to assess.
- Guest-write Samba share and default discoverable Bluetooth are deliberate appliance conveniences, not safe public-network defaults.
- MPD symlink following and some artwork absolute paths deserve boundary testing.
- Source changes and JSON stores have nontransactional corners; user-data corruption/rebuild recovery needs additional tests.
- Error strings/logs can contain paths, network identifiers or device data. Redact before export.
- Installer/update scripts execute as root. Native must never run as root merely to avoid fixing permission boundaries.
- Some sudo rules allow fairly powerful operations such as systemd-run. Audit argument restriction, ownership and writable script paths before declaring least privilege achieved.
- Base Pi mixer placeholder/output-path discrepancies in section 12 could affect correctness; do not hide them behind a passed API request.
- External PiShrink script is downloaded from a moving upstream branch by workflows; base Pi images have pinned URL but no clear complete supply-chain verification chain. Future hardening should pin/verify downloaded tools and base artifacts.

### 23.3 License and secret policy

Project root LICENSE is the MIT license; preserve it in the transferred snapshot. Review vendored Three.js, GSAP, Lucide, images and any new Qt licensing obligations separately. `THIRD_PARTY_NOTICES.md` explicitly covers Lucide paths but is not proof all vendored assets have a complete license inventory.

Do not copy device `/etc/pitunes`, saved NetworkManager profiles, SSH keys, updater signing keys, credential-helper output, access tokens or local personal library data into the native repository. Source templates can name credential fields without containing actual credentials. Avoid printing secrets in build or diagnostics logs.

## 24. Raspberry Pi Image Generation and Release Pipelines

Primary path `tools/build-flashable-image.sh`, base `image/build.env`, install `tools/chroot-install.sh` + root install scripts. Runs on Linux with root and QEMU user emulation for ARM, grows/mounts disk, installs scoped source and appliance packages, optional kiosk, golden cleanup, shrinks/compresses, produces armhf/arm64 images. Source-identity/installed-file checks reduce wrong-source releases.

`chroot-install.sh` creates a login user, randomized undisclosed password unless explicitly supplied securely, installs userconf hash for supported Pi setup and SSH policy, runs install/kiosk/cleanup. Do not reproduce temporary credentials or hashes. The user wants UI autologin, which is distinct from exposing an SSH password.

`golden-image-cleanup.sh` removes caches, logs/identity pieces, SSH host keys, saved Wi-Fi profiles unless requested, and staging source. It does not prove every log/user-data location is scrubbed. Verify image content before public release, including Chromium profiles and any new Qt settings store.

`release-image.yml` triggers only tags `v*`. It validates tag matches config version, runs selected Python/JS tests, builds both Pi architectures on ubuntu-24.04, validates compression/checksums, uploads artifacts, then publishes both release assets and marks stable Latest. Hardware boot/audio/OTA rollback are explicitly not exercised by this pipeline.

`build-image.yml` is a manual workflow with architecture/kiosk inputs. `tools/publish-image-release.sh` is a separate maintainer publication helper that requires clean matching source/tag. `tools/create-image.sh` is an imaging utility, not a safe generic PC installer. A/B builder remains a separate explicit staging step.

## 25. Intel/AMD Image Generation and Release Pipelines

`tools/build-pc-image.sh` requires root on x86_64 Linux, debootstrap and filesystem/GRUB tools. Builds a new temporary 5 GiB disk file, refuses preexisting output paths, creates MBR+ESP+ext4, bootstraps Debian with main and non-free-firmware, installs kernel, firmware, services and kiosk, then applies PC boot configuration and golden cleanup.

Staging patches in order:

1. `runtime.patch`: x86 device/route handling, PC system info/update gate, pc-alsa MPD mode, Pi installer exceptions, root-disk exclusion, kiosk/package adjustments.
2. `render-on-demand.patch`: stop unconditional WebGL redraw while idle, invalidate on art/data/navigation changes.
3. `kiosk-readiness.patch`: wait for API and MPD-backed status before opening Chromium.

`boot/configure.sh` independently installs display fallback/report, makes optional services off, embeds GRUB UUID search, adds compatible-graphics menu, and regenerates grub.cfg. Builder writes FAT `PITUNES-BUILD.txt` with architecture/source SHA and validates installed files.

`.github/workflows/build-pc-image.yml` is workflow_dispatch, GitHub-hosted ubuntu-24.04, parallel amd64/i386 matrix, 75-minute ceiling. It creates filenames `pitunes-<arch>-<short SHA>.img.xz` and checksums, runs `tools/test-pc-image.py`, uploads downloadable image artifacts only on success, and diagnostic artifacts on failure. Artifact retention is 14 days. It does not automatically create a new public release or update website links.

The test uses known-bootable QEMU hardware and disposable overlays; BIOS includes a separate internal disk, no NIC for offline kiosk test, standard VGA; UEFI uses correct 32/64 OVMF. Test music is generated silence WAV, so passed decoding is not a listening/quality test. It uses OCR to see the OfflineCheck album, API to verify native playback time advances, and a deliberately failing Xorg config on a disposable disk for bounded failure/report extraction.

The failed-display copy is made from the built image; downloadable image is not modified for fault injection. The test does not prove GPU driver correctness on the owner's laptop, fully test NAS/radio/physical USB DAC, or validate performance at 1 GB.

Original PC prerelease `pc-v1.4.4-preview.1` is a different source/artifact from successful Actions run 5. Publishing a normal stable PC release as Latest would interfere with Pi OTA unless release channel architecture is redesigned.

## 26. GitHub Actions, Compilation, Caching and Build Problems

### 26.1 Verified build history relevant to current state

| Commit / run | Result and lesson |
|---|---|
| `9920a40` | Pi build runner pinned to Ubuntu 24.04 for expected QEMU static binaries |
| `4bccf05` | Added PC recovery and optional-service behavior; build artifact alone did not update public preview |
| `a411b7b` | Added post-build PC boot gates and source-SHA filenames |
| `31a760c` | Fixed missing shell line continuation in apt install workflow |
| `903c2ff` | Removed redundant 5 GB decompress comparison and increased test timeouts |
| Run `45823829534` | i386 offline kiosk passed, failed services KeyError after too-short API timeout; amd64 Chromium renderer crashed |
| `e08dc50`, run `45859144184` | Forced software graphics / conservative emulated CPU; both offline kiosk + UEFI playback passed; no-display hardware test failed to reach API |
| `dd34c46`, run `45865676142` | Replaced missing-video-hardware test with forced Xorg failure on bootable disposable copy; both jobs completed successfully |

The conservative amd64 emulated CPU was changed from max to Nehalem in test tooling. This is a test-environment choice, not evidence a real CPU is Nehalem or a guarantee old hardware meets browser requirements.

### 26.2 Timeouts and tests

`/api/services` spawns systemctl calls for multiple components; emulated guest responses exceeded four seconds. The test now gives service reads an explicit longer timeout and waits for a valid payload. Top-level kiosk active is aggregated inactive; inspect component active=failed. These were test-contract bugs, not reasons to remove service health checks.

No fresh Qt compilation, compiler cache or cross-compiled Qt SDK exists in this repository. Existing code is Python/JS/shell; a native CMake/Qt build matrix must be introduced in the independent project.

Pi caches base image downloads under ignored build paths; PC uses fresh Debian suite repositories. No established native ccache policy exists. Cache immutable inputs keyed by arch/compiler/Qt version, never cache private credentials or silently reuse an old build as a new commit artifact.

The owner explicitly rejected self-hosted runners. All current PC workflow jobs specify `ubuntu-24.04` GitHub-hosted runners. Earlier local Mac x86 emulation was a testing/building workaround, not a requirement to preserve. A downloaded UTM framework loader later failed due to missing @rpath; do not spend the native project's release path recreating that custom Mac toolchain.

### 26.3 Operational lessons

Use branch/commit/build/version identity visibly. A GitHub source push is not publication of a disk image. A Compare & pull request banner is not a failed push. Force push is not a build repair. Keep tests and diagnostics in Actions; do not tell a user to flash a failed run's incomplete output. Avoid relying on expiring artifacts as permanent release URLs.

## 27. Current Known Bugs and Unresolved Issues

| Issue | State / next investigation |
|---|---|
| 1 GB 32-bit PC unusably laggy | USER-REPORTED after successful boot; no measured CPU/RSS/swap profile; primary native migration driver |
| Physical PC repeatable cold boot across all graphics modes | Not fully established; recovery VM success is narrower |
| Old Preview 1 remains public download | Verified release source ce972f4; newer dd34c46 is Actions artifact, not matching public preview |
| PC internal-disk installer missing | By design in current code, but user expected it; native project needs explicit decision and safe installer work |
| External source manager absent | Optional input_sources import is unfulfilled; receiver services != unified control |
| Missing b43 firmware on user PC | Documented licensing/device-specific issue; offline playback should remain usable |
| ACPI BIOS warnings | Hardware firmware messages, not removed; do not assert safe universal suppression |
| Pi mixer/device generation gaps | Quoted mixer placeholder, limited ALSA env use and suspicious KMS restore substitution need hardware verification |
| Album-art precedence discrepancy | embedded-first docs/settings vs folder-first route behavior |
| Favourites may depend on rebuild-unstable album IDs | Numeric index identity needs persistence/reconciliation design |
| Swapping local drives at same mount path | Source reset detection may miss device-only change |
| Settings two-store typing | SQLite string values overlay JSON native types; normalize/validate API |
| Large-library memory/scan load | Existing-file map, query repair, client song cap and textures; no exhaustive 1 GB library-scale validation |
| Smart playlists incomplete metadata | Frontend-only rules expose fields absent from backend schema |
| Full playlist management missing | Create/add implemented; native rename/delete/reorder requires API extension |
| Rating edit missing | Filter/schema exist, setter not found |
| Security hardening incomplete | Trusted-LAN model, no independent completed review, risks in section 23 |
| App OTA power-loss behavior | Archives and health rollback exist, not complete rootfs transaction |
| Signed A/B production readiness | Runtime/tests present, physical power-loss/watchdog matrix pending |
| System info memory available mislabel | Source parses free column as available |
| Docs contain stale roadmap/boot/testing descriptions | Reconcile using source; ROADMAP still lists already published releases as pending |
| Native implementation absent | No Qt code exists in original baseline; do not claim it is already ported |

## 28. Features That Must Never Be Removed or Broken

This is the migration acceptance checklist, not a demand to reproduce every accidental UI limitation:

- Offline boot with local display and no login/network/peripheral requirements.
- Always-on display while idle, paused and playing.
- Native local playback independent of GUI lifetime.
- Correct album grouping, artwork matching, track artist vs album artist and Unicode metadata.
- Album/song/artist/composer/year/genre/favourites browse, playlists, radio and usable search.
- Immediate selected-track start, full queue semantics, next/previous, seek, volume, repeat/shuffle and backend album continuation.
- No mid-track restart from polls/scans/queue fills.
- Persisted favourites, playlists and radio across reboot/source refresh/update.
- Explicit smart-playlist migration rather than silent loss of browser-only data.
- CSV exact filenames/paths, selected source memberships, cancellation and formula-safe quoting.
- USB/internal/NAS source picker and clear source unavailable/retry/reselect behavior.
- ALSA/USB DAC configuration, Pi-only HAT behavior and honest reboot warnings.
- Optional receiver/network/server capabilities without offline boot penalties.
- Touch/mouse/keyboard, readable text, dialogs and on-screen keyboard within active display.
- Diagnostics users can retrieve without a functioning GUI/SSH.
- Safe update boundary and prior-version recovery on supported failures.
- Separate Pi/PC builds and original/native update channels.
- GitHub-hosted reproducible source-identified builds, checksums, truthful test scope.
- Original Pi installation remains functional and unchanged during native development.

## 29. Source Files, Functions and Modules by Feature

| Ownership | Start here |
|---|---|
| HTTP contracts | `backend/server.py:Handler`, `compat_*`, `POST_ACTIONS`, `SIMPLE_MPD_POSTS` |
| API runtime settings/paths | `backend/shared.py`, `server.py:load_settings/write_settings` |
| MPD protocol | `backend/mpd_client.py:MPDClient.command/entries/single_map/binary` |
| Queue construction | `backend/playback.py:play_queue_fast`, `_mpd_append_uris_async`, `mpd_add_uri` |
| Album continuation | `next_album_id_after`, `maybe_continue_next_album`, `MPDIdleWatcher`, `ContinuousPlaybackPoller` |
| Repeat/shuffle | `backend/playback_modes.py:mode_commands`, `frontend/assets/playback-order.js` |
| Schema | `backend/library/db.py:init_db/_migrate_schema/get_connection` |
| Scanner | `scanner.py:run_scan/start_scan/_folder_album_metadata/_get_or_create_album/_index_album_folder` |
| Browse/search | `queries.py:list_albums/list_all_tracks/album_tracks/list_starred_tracks/search_all` |
| Album identity | `frontend/assets/album-identity.js`, scanner identity tests |
| Artwork | `art_resolver.py`, `server.py:get_art_file/embedded_thumb_from_track/virtual_cover_file` |
| Favourites/playlists | `userdata.py`, `server.py:post_favourites/post_create_playlist/compat_playlist_tracks` |
| Radio directory/icons | `radio_browser.py:search_stations/lookup_favicon`, `radio_icons.py` |
| Shared UI state | `library/ui_context.py:publish/get/publish_playback`, `app.js:syncBrowseFromRemoteContext` |
| Source mounting | `storage_sources.py`, `mount-music-drive.sh`, udev and mount/refresh units |
| Wi-Fi/AP | `network_wifi.py:wifi_status/wifi_scan/wifi_connect`, `setup-wifi.sh`, `wifi-hotspot.sh` |
| Output configuration | `audio_output.py:audio_devices_payload/apply_audio_output`, `configure-mpd.sh`, `apply-audio-output.sh`, DAC JSON |
| Receivers | `setup-wireless-audio.sh`, Bluetooth units, `bluetooth-discoverable.sh`; missing input_sources noted |
| Service/system API | `server.py:service_state/control_service`, `library/system.py:get_info/check_update/apply_update` |
| App OTA | `scripts/pitunes-update.sh`, update service, test_update_script.sh |
| A/B | `scripts/pitunes-system-update.sh`, boot-check, bundle/AB builders, AB docs/tests |
| CoverFlow | `renderer.js:SlideCard/initScene/navigateTo/jumpTo/refitStage`, `coverflow.js` |
| Browse UI | `app.js:loadAlbumBrowse/loadSongBrowse/loadArtistAlbums/loadComposerAlbums/loadRegularPlaylist/loadSmartPlaylist` |
| Drawer/info | `fetchAlbumTracksForDrawer`, `prepareDrawerContext`, `setDrawerOpen`, `renderSongsDrawer`, `buildAlbumInfoRows` |
| Playback UI | `refreshPlayer`, `playAlbum`, `postFastAlbumPlayback`, `playBrowserTrack`, `updatePlaybackUi` |
| Layout/touch | `layoutPlayer`, `positionReflectionStack`, `syncPlaybackStripLayout`, keyboard helpers, `bindEvents` |
| Export | `music-export.js:collectMusicLists/buildMusicListCsv`, UI export dialog |
| PC adaptation | `image/pc/*.patch`, `image/pc/boot/*` |
| Boot | `setup-kiosk*.sh`, `pitunes-kiosk-launch.sh`, `firstboot.sh`, systemd and drop-ins |
| Builds/deploy | `tools/build-*.sh`, `chroot-install.sh`, `deploy-to-pi.sh`, workflows |
| Mock | `tools/mock-server.py`, `tools/start-mock.ps1` |
| Branding/legal | `config/brand`, frontend/docs assets, LICENSE, THIRD_PARTY_NOTICES.md |

## 30. Recommended Migration Strategy to C++17 / Qt Widgets

### Phase 0: Freeze reference and establish contracts

Use this source SHA as the reference snapshot, not a moving main branch. Retain original repository unchanged. Record feature parity matrix and fixtures from section 19. Establish real target CPU/GPU/RAM, display/input and audio route. Choose Qt version/toolchain using actual distro 32-bit availability; no Qt major version is fixed by this handoff.

### Phase 1: Minimal native shell, existing backend

Create CMake/C++17 project with Qt Widgets, Qt Network and Qt Test where available. Start with main window, source/settings access, album list, track list and transport. Run the existing Python backend and MPD unchanged on a staging machine. Use QNetworkAccessManager asynchronously and normalize API responses in one adapter.

Keep native code unprivileged. Do not spawn shell mount/systemctl commands from paint handlers. Reuse privileged API contracts for actual appliance operations, pending a separate security review.

### Phase 2: Models, state and concurrency

Suggested components (new design, not current filenames):

```text
ApiClient             request/response, errors, timeouts, cancellation
PlayerController      canonical backend player state and user intents
LibraryModel          paginated albums/tracks, stable IDs, source generation
BrowseState           local selection vs playing item, remote context sync
ArtworkCache          bounded memory/disk, async decode, revision keys
SourceController      local/internal/NAS selection and availability
SettingsController    hardware/services/network/update operation states
CoverFlowWidget       presentation/input only, QPainter rendering
TrackListModel        model/view song rows and actions
```

Never block GUI thread on network, metadata, artwork decode, directory scan, service enumeration, NAS mount or update polling. Drop stale replies after source/selection change. Keep elapsed clock extrapolation separate from full model refresh. Preserve backend queue ownership.

### Phase 3: QPainter CoverFlow

Implement cached QImage/QPixmap projection with QTransform, clipped dirty regions, reflected images precomputed per art revision/size, and a timer only during an active transition. Work with a small visible window, not the entire library. Use device-pixel ratio correctly without multiplying cache size on tiny displays. Match move/rotation/scale feel and snap behavior with reference screenshots/recordings, while providing simple low-cost display mode.

Do not call update() continuously at idle, regenerate reflections every frame, perform smooth high-resolution scaling of unseen covers, or allocate widgets per song. A software QPainter path still needs profiling on the 1 GB device.

### Phase 4: Complete parity and persistence

Port favourites, regular/smart playlists, radio, metadata drawer, source picker, output apply, network setup, service toggles, system info, CSV and modal keyboard. Decide where smart playlists/per-client preferences live; supply import from browser data rather than silently discarding it.

Shared UI context should keep optional remote web client and native display consistent, without forcing all clients to follow every local selection unless the existing policy requires it. Preserve original web frontend as optional LAN control initially; replacing local Chromium does not require removing nginx/web APIs.

### Phase 5: Independent appliance integration

Create native display session/unit and native application health evidence. Test no-network, no-Bluetooth, no-NAS startup. Keep diagnostics on FAT boot partition. Add safe install-to-internal-disk only as an explicit separate feature with target selection, destructive confirmation and backup guidance; a file copy is not an installer.

Reuse architecture-specific image mechanics, but replace kiosk package/launch steps deliberately. Keep original ARM builds out of the native repository's automatic release path. Native App OTA needs a separate binary/dependency-aware transaction and release identity; original updater is not already suitable.

### Phase 6: Hardware acceptance before public claims

Compare original Pi edition and native Pi on the same hardware. Test actual 32-bit 1 GB PC cold boot and prolonged playback, input latency and memory, not just build success. Publish only evidence-backed claims and known limitations. Keep rollbackable old images for staging; do not erase the owner's Windows installation to diagnose native performance.

## 31. Components Reusable Without a Rewrite

Subject to the explicit known gaps above:

- MPD as native local/radio audio engine and queue authority.
- Python HTTP API and most library/userdata/query behavior.
- Mutagen scanner, file/album identity rules and duration fallback.
- Artwork extractors/proxy/cache and generated placeholder concepts.
- JSON favourites/playlists/radio persistence and fixtures.
- NetworkManager tools, asynchronous Wi-Fi handoff and scan cache.
- USB/internal/NAS mount helpers, with platform/path/timeout audits.
- ALSA device discovery and existing apply helper, after reviewing Pi-specific correctness gaps.
- Receiver service setup, without claiming missing unified source manager.
- Transactional App OTA concepts and signed A/B safety model, not native or PC channel support as-is.
- ARM/PC image creation mechanics and source identity checks.
- Branding, legal notices, expected menus and behavioral regression tests.
- Optional LAN web UI for compatibility while native local UI is introduced.

Prefer extracting pure interfaces over rewriting every daemon in C++. The new requirement is a native display, not necessarily replacing the proven audio/indexing stack.

## 32. Components That Need Native Redesign

- Browser DOM/CSS layout, modals, portals, touch keyboard placement and focus management.
- Three.js/GSAP WebGL scene, GPU/software-render flags and HTML canvas hit testing.
- Browser localStorage preferences and smart playlists, including import strategy.
- HTML audio/browser-output mode. Optional remote browser playback can remain in web frontend; native appliance should keep MPD.
- JS player request races and UI-state reconciliation into a typed controller layer.
- CSV file save/download via Qt file dialog and streaming write while retaining exact format/security rules.
- Browser-only fullscreen and pointer/touch heuristics into platform-native behavior.
- Chromium launch/LightDM autostart ownership into native session/service and health reporting.
- Product/version/update channel and native binary/package rollback.
- Hardware capability reporting and UI enablement: absent NIC/controller/DAC is not a boot failure.
- Full paired-Bluetooth device management and actual source arbitration, if required for parity; these are not already complete in old code.
- Internal-disk installer if desired; current bootable USB cannot be presented as one.
- Performance tests at 1 GB and resource budgets, absent in original acceptance tests.

## 33. Complete Testing and Validation Requirements

### 33.1 Existing checks

Commands for a suitable development environment:

```sh
./tools/validate-release.sh
python3 -m unittest tests.test_album_scanner_identity tests.test_music_export_queries \
  tests.test_userdata_playlists tests.test_system_update tests.test_playback_modes
node --test tests/*.test.mjs
```

Validation requires Python >=3.10. This Mac's `/usr/bin/python3` was 3.9.6; the bundled Codex Python 3.12 runtime was used for the fresh selected unit tests. Do not change application dependencies merely to work around a development-host interpreter.

**Fresh during this handoff**: selected Python suite passed 20 tests; JavaScript suite passed 24 tests; `tools/validate-release.sh` passed including App OTA script checks, signed system-update checks and system boot-check rollback fixtures. No physical hardware or image build was run for documentation. Previous completed release/PC workflow results are historical evidence, identified separately. The document's structure and literal API route references were also checked for coverage.

`test_radio_search.py` is a top-level mock-server integration probe at localhost:8095, not a self-contained unittest. Running broad discovery can contact that server unexpectedly. Mock server serves real frontend with fake playback and is not audio verification.

Shell fixtures `test_update_script.sh`, `test_system_update_script.sh`, `test_system_boot_check.sh` exercise updater safety paths using stubs. They do not perform a physical power-cut test.

### 33.2 Existing test coverage map

| Tests | Behavior |
|---|---|
| `test_album_scanner_identity.py`, `album-identity.test.mjs` | Same title distinct artist/year, equivalent identity, numeric IDs authoritative |
| `test_music_export_queries.py` | Large favourite query, composer metadata |
| `test_userdata_playlists.py` | Single-write bulk playlist create, compatibility |
| `test_playback_modes.py`, `playback-order.test.mjs` | Repeat/shuffle mapping, invalid options, radio rejection, full queue retention, async cancellation, no stale state rollback |
| `browse-counts.test.mjs` | Correct totals, selected panel, smart playlist count, grouped song totals |
| `music-export.test.mjs` | Dedup, CSV escaping/formula defense, unselected sources not fetched |
| `test_system_update.py` | Version comparison, stable release/SHA, prerelease rejection, descriptor routing, capability gate |
| updater shell tests | Archive/lock/update safety and boot rollback guard fixtures |
| `tools/test-pc-image.py` | Offline BIOS painted album, UEFI MPD playback, forced-Xorg failure with report and live backend |

### 33.3 Native automated requirements

- API adapter fixtures for every route group, mixed types and missing keys.
- Model pagination with empty/partial/full/large libraries and stable IDs across sorting.
- Unicode Tamil/non-Latin metadata, long titles, compilation singers, same-title reissues, missing tags/art.
- Queue parity tests, rapid repeated clicks, source switch during queue fill, stale responses, pause/resume, repeated Next, browser disconnect.
- QTest keyboard/touch/pointer state tests, focus traps, on-screen keyboard and modal shielding.
- Golden screenshots at small Pi display, old laptop resolution, desktop and rotated/portrait view; long text never occludes controls.
- Dirty-region/idle timer tests, bounded artwork cache and async cancellation.
- Persistence export/import tests including browser smart playlists and settings schema migration.
- Crash-restart native process while MPD continues; backend unavailable at launch; native display failure report.
- Packaging/install/upgrade/rollback test of exact released Qt binary, linked libraries and supported runtime arch.

### 33.4 Physical acceptance matrix

- Supported Pi models/architectures and actual old 32-bit 1 GB PC.
- Cold start vs reboot, no network hardware, no Bluetooth, no music, empty/music USB, insert/remove/reinsert, multiple disks/internal Windows present.
- BIOS/UEFI and safe graphics; USB2/USB3/SSD performance; actual touch and keyboard.
- MP3/FLAC/WAV/AIFF/AAC/M4A/ALAC/OGG/Opus fixtures as supported; missing tags, cover extraction, multiple discs.
- Real USB DAC enumeration and reboot-order changes; Pi HAT overlay round trips; HDMI/headphones where physically available.
- Long uninterrupted audio while scanning/exporting/browsing; no underruns/restarts; actual listening tests, not silence-WAV elapsed alone.
- NetworkManager Wi-Fi/AP handoff, wrong password, hotspot recovery, wired route, absent firmware, NAS unreachable/ICMP-blocked/credentials failure.
- Receiver playback and transitions with actual phones, default ALSA contention, enable/disable and shutdown latency.
- Sustained idle/playing display stays awake.
- Update UI locked through API restart and client disconnect, failure logs, rollback, persistent data integrity; A/B power-loss matrix on staging only.
- Clean-image privacy inspection and independent security review before broader claims.

### 33.5 Acceptance evidence

Every candidate needs product/version/source SHA, artifact name/hash, architecture/base/package manifest, test-run URL, screenshot/log evidence, hardware model and known failures. A screenshot showing a rendered album proves neither input latency nor audio quality. A checksum proves file identity, not that firmware can boot or a DAC works.

## 34. Unpublished Local Changes and Transfer Boundaries

At the start of this handoff the tracked worktree was clean, `main` matched `origin/main`, and HEAD was `dd34c4635a523b8dabc40e976f2d7b11abf88885`. No unpublished tracked code edits or preexisting untracked nonignored source files were reported by Git at inspection.

Ignored local generated artifacts included original Nov 21 compressed amd64/i386 images with matching checksums in `image/out`, and `image/work/pitunes-i386-recovery.img`, a 5 GiB working raw image. Those filenames are not proof of current source or completed recovery. Temporary `/tmp` VMs/downloads/helpers and local credentials are not part of the handoff and are not to be transferred as source.

This handoff creates only this Markdown file inside the original repository, without modifying application behavior, workflows, releases, OTA, tags or remote history. It is intentionally not committed or pushed.

The owner subsequently requested the files in the new native workspace. The safe transfer is a **copy of tracked source plus this document**, not deletion/move of the working original and not copying `.git`. The destination was empty when checked. The original repository remains intact. Generated images, caches, backups, local credential material and Git internals are excluded. Original tracked workflows/updaters are reference material only; copying them does not make them safe native release automation.

The new folder must not inherit the old Git remote or default update repository. Initialize/publish the native project deliberately later, after product/channel isolation; this documentation task does not do so.

## 35. Latest Project State, Source and Releases

### 35.1 Source identity

- Original project: `BASF2HD/PiTunes`.
- Baseline HEAD and origin/main: `dd34c4635a523b8dabc40e976f2d7b11abf88885`.
- Application config: version `1.4.4`, channel stable.
- Public Pi release `v1.4.4` points to `9920a4042759fccef76a5b8f20deeb433b069323`, not current HEAD.
- Public PC tag `pc-v1.4.4-preview.1` peels to `ce972f473d0f01a7d8c7d8436ac0177dfa58baa3`, not current HEAD.
- The new native GitHub repository page was empty at read-only inspection. Local destination folder was empty before transfer. No native implementation should be assumed.

### 35.2 Verified public releases as inspected 2026-11-26

| Tag | Published UTC | Notes |
|---|---|---|
| `pc-v1.4.4-preview.1` | 2026-11-21 19:14:34 | PC prerelease, amd64/i386 image + checksum each, original preview |
| `v1.4.4` | 2026-11-15 23:06:07 | Latest Pi stable, armhf/arm64 + checksum each |
| `v1.4.3` | 2026-10-11 18:47:40 | Menu totals/compact rows |
| `v1.4.2` | 2026-10-06 22:00:45 | Repeat/shuffle and complete-queue semantics |
| `v1.4.1` | 2026-09-29 21:27:54 | Album release identity, faster favourites/playlists |
| `v1.4.0` | 2026-09-19 19:19:59 | CSV export/project-page work |
| `v0.2.0` | 2026-06-10 10:20:53 | Earlier arm64 image ZIP |

### 35.3 Successful newer PC candidate

[GitHub Actions run 45865676142](https://github.com/BASF2HD/PiTunes/actions/runs/45865676142), source `dd34c46`, completed successfully for both architectures. Artifacts inspected as present/nonexpired at preparation:

| Artifact | ID |
|---|---|
| `pitunes-amd64-usb-5` | `15141212652` |
| `pitunes-i386-usb-5` | `15140553778` |
| `pc-boot-tests-amd64-5` | `15140513953` |
| `pc-boot-tests-i386-5` | `15140418898` |

Images use source-stamped names such as `pitunes-i386-dd34c46.img.xz`. Artifact links expire under configured retention; they are not permanent public release assets. Original website/README still link Preview 1. User successfully wrote the newer image with Raspberry Pi Imager after Etcher writer failure; downloaded i386 checksum was verified, so no confirmed archive corruption was established.

**Final physical status**: PC boot achieved, low-memory performance failed user acceptance. Raspberry Pi performance was reported good. Native development exists to solve the local UI resource burden while preserving backend/audio/library behavior, not to erase the evidence of this failure.

## Supporting Evidence and Further Reading

Repository-relative source references above travel with the copied snapshot. Key documents: `README.md`, `backend/README.md`, `CHANGELOG.md`, `SECURITY.md`, `docs/PC_USB.md`, `docs/UPDATES.md`, `docs/AB_SYSTEM_UPDATES.md`, `docs/BOOT_PERFORMANCE.md`, `docs/MAINTENANCE.md`, `docs/IMAGE_CREATION.md`, `docs/RADIO.md`, `docs/WIFI_HOTSPOT.md`. They are supporting material, not a replacement for current source inspection.

Public evidence links used during preparation:

- [Original releases](https://github.com/BASF2HD/PiTunes/releases)
- [Latest successful PC workflow](https://github.com/BASF2HD/PiTunes/actions/runs/45865676142)
- [Native repository](https://github.com/BASF2HD/PiTunes-Native)
- [GNU GRUB rescue behavior](https://www.gnu.org/software/grub/manual/grub/html_node/GRUB-only-offers-a-rescue-shell.html)
- [GNU GRUB embedded configuration](https://www.gnu.org/software/grub/manual/grub/html_node/Embedded-configuration.html)
- [Linux Wireless b43 firmware and redistribution](https://wireless.docs.kernel.org/en/latest/en/users/drivers/b43/firmware.html)
- [Debian Broadcom setup](https://wiki.debian.org/bcm43xx)
- [Raspberry Pi Imager custom-image documentation](https://www.raspberrypi.com/documentation/computers/imager.html)

The available conversation includes PiTunes, Raspberry Pi appliance work and a separate Navidrome/Coverflow discussion. Do not assume `/Users/oomayan/Documents/navidrome_coverflow` is this repository or that Navidrome authentication/API code exists here. Those older paths and credentials are not part of this transfer. The architectural goal is parity with the verified PiTunes behavior described above.
