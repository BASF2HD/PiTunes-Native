# Inherited project context

Knowledge audit: 26 November 2026 (Europe/London).

## Source and transfer review

Original: https://github.com/BASF2HD/PiTunes at
`dd34c4635a523b8dabc40e976f2d7b11abf88885`, version 1.4.4. The sibling original
was inspected read-only. Live GitHub main/HEAD match this SHA. All 174 tracked
local files match the live GitHub recursive tree's Git blob IDs and the native
folder's initial copied files match the pinned commit byte-for-byte. The original
has no tracked modifications or unpublished commits relative to live main. Its
only nonignored untracked file is `PITUNES_KNOWLEDGE_TRANSFER.md`, already supplied
here; reviewed in full as engineering narrative, not executable code. Ignored
images/caches and machine state were excluded. No unpublished code was imported.
`docs/inheritance/original-source-sha256.json` records the pre-adaptation files.

The destination was populated but had no `.git`; its live GitHub repository was
empty. A fresh main repository was initialized here with only the native remote.
The original `.git`, runtime credentials and generated artifacts were not copied.

## Engineering purpose

The original Pi appliance reportedly performs well. Its latest PC image boots but
the owner's 32-bit 1 GB PC has unacceptable input latency. No instrumented root
cause exists. SwiftShader solved a VM renderer crash, not the hardware performance
problem. Native replaces the local display, preserving audio/library behavior and
appearance; it is not an audio engine rewrite or an internal-disk installer.

## Authorities and data

Qt -> asynchronous HTTP API at loopback :8080 (or nginx :80) -> Python standard
library ThreadingHTTPServer -> MPD :6600 -> ALSA. SQLite schema 5, WAL, FTS5 owns
browse/search. MPD owns decoding/queue/repeat/random and continues with no UI.
Remote browsers retain `frontend/`, served by nginx. No second music database.

Runtime stores: `/etc/pitunes/settings.json`, `/etc/pitunes/userdata.json`
(favourites, ordered playlists, stations), `/var/cache/pitunes/library.db`, art
cache, shared `ui-context.json`, MPD state, `/mnt/music`. Browser smart playlists
and per-client preferences are separate localStorage data. Do not reset the cache
or browser profile as a migration strategy: album favourite IDs can change on a
destructive rebuild. NAS config/credential files and Wi-Fi profiles are sensitive.

## Contracts and historical decisions

- Numeric album ID + album artist/year disambiguate releases; track ID is its
  relative file URI. Preserve track artist independently of album artist.
- Start selected audio immediately; append queue asynchronously and prepend prior
  entries without restart. Backend idle watcher plus backup poller own continuation.
  GUI must never duplicate it or reset the queue on refresh.
- Drop stale asynchronous results. Wrong covers/drawers and mid-track resets are
  historical regression classes; keep identity separate from browse position.
- API uses mixed JSON types/naming; body may report `ok:false` or `error` with HTTP
  200. No event feed exists; bounded asynchronous status polling is necessary.
- USB/internal/SMB/NFS, NetworkManager hotspot, ALSA routes and optional receivers
  remain backend responsibilities. USB playback must work without networking.
- Browser smart playlists expose fields the schema does not supply. Preserve
  definitions, flag unsupported rules, never fabricate metadata.
- UI baseline: original `docs/assets/pitunes-player-gui.png`, black background,
  white brand, centered cover, angled sides, faded reflections, compact menus,
  centered artist/year and transport. Screenshot is a visual reference, not proof
  that the mock library contains that art. Original renderer uses 52-degree sides,
  move .42 s/rotation .28 s/scale .32 s; no unconditional refit during transitions.
- Native OTA must be disabled and independent. Original app OTA is source/archive
  based; signed Pi tryboot A/B is capability-gated and physically unvalidated.

## Evidence and releases

Live releases: Pi stable `v1.4.4` at `9920a40` (15 November), PC prerelease
`pc-v1.4.4-preview.1` at `ce972f4` (21 November). Neither equals current main.
Original Actions [run 45865676142](https://github.com/BASF2HD/PiTunes/actions/runs/45865676142)
at dd34c46 succeeded for both PC architectures. Its QEMU/OCR/silence-WAV tests do
not establish real DAC quality, 1 GB responsiveness or physical GPU compatibility.
An owner-reported physical i386 USB boot exposed hardware and input issues. Physical performance measurements remain outstanding. See [Atom hardware observations](ATOM_HARDWARE.md).

Read ARCHITECTURE_AUDIT for source ownership, ARCHITECTURE_DECISIONS for choices,
KNOWN_ISSUES for gaps, FEATURE_PARITY for migration status and PROGRESS to resume.
