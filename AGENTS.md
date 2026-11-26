# PiTunes Native permanent engineering instructions

## Read before changing code

Read `docs/PROJECT_CONTEXT.md`, `docs/ARCHITECTURE_DECISIONS.md`,
`docs/KNOWN_ISSUES.md`, `docs/FEATURE_PARITY.md`, and `docs/PROGRESS.md` first.
Read `PITUNES_KNOWLEDGE_TRANSFER.md` in full on the first session, then its
relevant sections before work on audio, scanning, persistence, images or updates.
Consult `docs/ARCHITECTURE_AUDIT.md` and the actual source for contracts. Original
documentation under `docs/reference/` describes historical behavior, not native
release readiness. Update progress and parity records with every milestone.

## Isolation and provenance

- Work only in PiTunes-Native. The sibling PiTunes directory and
  `BASF2HD/PiTunes` are strictly read-only: never edit, fetch into, commit, push,
  tag, publish, or change configuration there. Use `GIT_OPTIONAL_LOCKS=0` for
  read-only status checks. Do not run its tests in place; run inherited tests here.
- Before any push check `pwd`, `git status`, branch and both remote URLs.
  Destination must be `https://github.com/BASF2HD/PiTunes-Native.git` or its
  equivalent SSH URL. No force pushes, destructive resets or nested repositories.
- Original reference is `dd34c4635a523b8dabc40e976f2d7b11abf88885`.
  Record any later inheritance with path, purpose, safety and validation. Never
  silently copy unpublished source, ignored files, images, personal data or secrets.
- Preserve MIT attribution and third-party notices. Reuse original branding.
  Never import keys, tokens, passwords, Wi-Fi/NAS credentials, real device settings,
  diagnostic dumps or music. Generated builds/images stay outside Git history.
- Image staging copies only Git-visible files from approved source roots, rejects
  symlinks/key/credential paths, and excludes ignored local environment files.
  Never replace this with an unrestricted recursive copy of the checkout.

## Architecture and compatibility

- Local interface is C++17 + Qt 5 Widgets + QPainter + CMake. No Chromium,
  Electron, WebEngine, WebView, CEF, HTML shell or mandatory OpenGL/WebGL.
- MPD/ALSA own audio and queue state; Python owns SQLite, scanner, artwork,
  favourites/playlists and appliance helpers. Qt uses asynchronous API calls and
  never writes the library database or runs privileged commands directly.
- Keep the remote `frontend/` web interface and nginx/API compatible. Remote
  browser output can remain. There must be no browser process on the appliance.
- Album numeric ID is authoritative; track identity is its relative file URI.
  Preserve album artist/year grouping, per-track singer, Unicode, queue prefix
  when starting mid-album, and backend-only automatic continuation. Polls, scans
  and artwork refresh must never reset audio or rebuild the playback queue.
- Every asynchronous view request needs generation/identity cancellation.
  Playback mutations must not be retried automatically after ambiguous failure.
- Keep artwork request/decode counts and memory strictly bounded; decode off the
  GUI thread at thumbnail sizes and cache reflections. Fit visible covers to
  the viewport within the artwork budget. Stop animation timers at rest and
  virtualize lists. Prioritize 1 GB Atom / 512 MB Zero 2 W.
- Offline first paint and local playback must not wait for network, NAS,
  receivers, scans or service enumeration. Display stays awake idle/paused/playing.
  GUI failure must leave MPD/API independent and recover with bounded restarts.
- Retain every user-visible feature. Track incomplete native work explicitly in
  FEATURE_PARITY. Do not discard browser smart playlists/localStorage: use an
  explicit, tested export/import path into shared storage, preserving originals.
- Four separate images: Debian 12 i386 (PAE baseline), Debian 13 amd64,
  Raspberry Pi OS Lite Bookworm armhf and arm64. Never substitute generic Debian
  for Pi OS; retain correct Pi firmware/overlays. No `-march=native`. Validate CPU
  requirements, BIOS and the relevant 32/64-bit UEFI independently.

## Safety, updates and evidence

- Native product/release/update identity is `BASF2HD/PiTunes-Native`. OTA remains
  disabled until binary/dependency installation and rollback are independently
  tested. Legacy updater fixtures are historical tests, not native OTA approval.
  Never use the original release feed or publish native assets there.
- Preserve settings/userdata/music through updates. Never partition or format an
  internal disk automatically. Image builders operate on new regular image files.
- Services run unprivileged with audited privileged backend helpers. Trusted LAN
  API has known security limits; do not expose it to the Internet.
- Use small testable commits; run relevant Python, JavaScript, C++ and integration
  checks. Avoid full image builds for ordinary C++ changes. Use GitHub-hosted CI.
- Never describe source push, compilation, emulator boot, physical boot and
  performance acceptance as equivalent. Record exact executed tests, versions,
  source SHA, errors and hardware blockers. Never invent benchmarks or claim
  bit-perfect output without measurement. Do not publish production images or
  enable stable OTA before the documented release gates pass.
