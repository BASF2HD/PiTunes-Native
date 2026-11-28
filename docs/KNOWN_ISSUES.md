# Known issues and historical failures

Historical evidence is inherited from dd34c46. The owner also completed the first physical i386 USB boot. See [Atom hardware observations](ATOM_HARDWARE.md).

| Issue | Evidence / status | Required action |
|---|---|---|
| Original 1 GB 32-bit PC effectively unusable after boot | Owner report; CPU/RSS/swap not captured | Native hardware acceptance, no speculative root cause claim. |
| GRUB normal.mod rescue | File existed; disk enumeration/UUID recovery added | Preserve embedded UUID search; test BIOS with internal disk present. |
| LightDM failed repeatedly | PC fallback/report at 4bccf05; dd34c46 fault test passed | Preserve bounded default/fbdev/VESA attempts and FAT diagnostics. |
| SwiftShader workaround | VM crash fixed; physical lag remains | No browser/software 3D stack in native image. |
| b43 firmware absent, ACPI warnings | Hardware/licensing; offline boot can still work | Do not redistribute restricted firmware or globally disable ACPI. |
| `input_sources.py` missing | Source inspection | Receivers exist; arbitration/metadata/takeover unverified, requires backend work. |
| Pi mixer placeholder / device handling | Literal `${MIXER_TYPE}` in quoted heredoc; fixed in PC patch only | Validate generated MPD config and HAT->HDMI restore before hardware claims. |
| ALSA numeric cards unstable | Device discovery uses card numbers | Stable identifiers/capability tests needed; no bit-perfect claim for plughw/software mixer. |
| Folder-first art vs embedded-first docs | `get_art_file` prefers folder in key path | Preserve actual behavior initially, reconcile deliberately with fixtures. |
| Rebuild changes numeric album IDs | Favourite IDs use DB IDs | Reconcile before destructive source/cache migrations. |
| Same mount path, different drive | Source reset checks path/source, not all device identities | Add device identity regression coverage. |
| JSON settings overlaid by SQLite strings | Existing dual store | Normalize at client boundary and test types. |
| Smart playlists browser-only | Working rules plus unsupported metadata fields | Explicit shared-store migration; keep original localStorage intact. |
| Playlist rename/delete/reorder absent | Only create/add API in original | Intentional compatible extension needed for full requested management. |
| Rating editing absent | Schema/filter only | Do not present a functioning rating setter until implemented. |
| No internal-disk installer | Existing images are persistent USB | Document clearly; never format internal media automatically. |
| API trusted-LAN security limits | Unauthenticated privileged controls, no general CSRF/TLS/rate limits | Security review before public-network exposure; Qt does not fix this. |
| Request/media bounds incomplete in old backend | Body size, URL proxy/SSRF, symlink/path risks | Track and test hardening without silently changing working contracts. |
| Userdata atomicity corners | UI context direct write, multiple settings authorities | Crash/power-cut and concurrency fixtures needed. |
| `free -h` available field wrong | Fixed in native adaptation with regression fixture | Target measurements still pending. |
| Signed Pi A/B incomplete physical validation | Script tests only | Power-loss/watchdog/tryboot matrix, no PC A/B claim. |
| Original workflows moving dependencies | PiShrink master, live Debian suites | Pin/verify base/toolchain inputs and record package manifests. |
| Native images/hardware/performance | Development images and PC VM evidence in PROGRESS. Atom USB boot confirmed, target performance unmeasured | Do not publish stable images or promised RAM/frame rates. |

Do not repeat: title-only grouping, per-singer albums, full-queue synchronous
startup, queue resets during polls, stale async drawer results, continuous idle
painting, network-online on display critical path, running Samba to read a NAS,
blind apt upgrades in App OTA, timeout increases without logs, removing all VM
video as the only display-failure test, or treating checksum/boot as usability.

## Native milestone limitations

Native album and all-songs shelves page in 200 entries at a time. Album paging
is tested through 5,001 entries. Starting playback from the song shelf queues
the loaded entries. Queue editor, complete playlist editing, native CSV
save, smart rule evaluation/builder, radio favourites/removal, shared browse-context
publishing/following, scan progress and complete preferences migration remain.
Settings forms compile but actual Linux privileges/receivers/mount/audio are not
validated. On-screen keyboard currently covers Latin letters/punctuation, not a
complete multilingual IME. Exact geometry/animation parity and physical touch
acceptance are still open. Imported smart definitions are safe but not executed.

Native images are built on GitHub-hosted root Linux; PC VM acceptance is recorded
in PROGRESS. Target DACs and physical target hardware are unavailable. No physical
boot or target resource acceptance is claimed. Legacy builders/install/deploy/publication are
fail-closed unless invoked through the explicit native staging/build path.

Low-memory artwork prefetch was reviewed for eviction loops: the desired window
is now bounded by cache capacity, and a valid-image regression test confirms
requests stop at idle even with a 4 MiB cache. Decoder transient memory is bounded
separately by two workers, source pixel ceiling and compressed byte limits.

Low-resolution keyboard/modal layouts need further QA with the keyboard open;
the checked 800x480/480x800 screenshots cover the main player without that overlay.
Generated e428678 image candidates precede the later stale-drawer/callback cleanup
corrections; source/application-package validation is tracked separately.

Qt 5 SVG Tiny does not support the backend placeholder's CSS rgba paints or
pattern fills. The native artwork worker now adapts rgba to RGB/opacity (tested)
and omits only the optional dotted texture of the known PiTunes generated cover.
The existing backend/remote placeholder stays unchanged; exact native texture
parity remains open. Native album playback passes its active sort and filter
to the continuation logic. Shared navigation context publishing and following
remain open.
