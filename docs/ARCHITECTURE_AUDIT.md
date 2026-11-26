# Original architecture audit and native boundary

Audit date: 26 November 2026. Reference: original PiTunes
`dd34c4635a523b8dabc40e976f2d7b11abf88885`. All 174 tracked files were compared
with live GitHub blobs; source inventory SHA-256 is in `docs/inheritance/`.
The full transfer was read, actual dispatch/scanner/query/queue/store/boot/image
paths inspected, and the original screenshot viewed. This records source evidence,
not a claim that all hardware behavior has been independently reproduced.

## Runtime and dependency map

| Layer | Sources | Responsibility and dependencies |
|---|---|---|
| Presentation | frontend/index.html, app.js, styles.css, renderer.js, coverflow.js | DOM menus/modals/touch keyboard, browser state, Three.js + GSAP art scene. Optional browser audio. Replace only local presentation. |
| HTTP | backend/server.py, shared.py, mpd_client.py | Python >=3.10 standard-library ThreadingHTTPServer, JSON route dispatch, thread-local MPD connections with one reconnect/retry. Pillow optional art dependency. No Flask. |
| Playback | playback.py, playback_modes.py | Fast selected-track start, async append/prefix, generation cancellation, MPD idle player/playlist watcher + 1.5 s continuation fallback. MPD owns actual queue/decoding. |
| Index | library/db.py, scanner.py, queries.py | SQLite schema 5/WAL/FTS5; Mutagen + wave metadata; folder/title/album-artist/year grouping; incremental traversal, stale deletion, scan generations/progress counts. |
| Artwork | art_resolver.py, server.get_art_file, radio_icons.py | Folder/embedded/virtual covers, 128/420 thumbnails, revision URLs, radio logo proxy/background cache. Documentation's embedded-first wording differs from key folder-first route. |
| User data | userdata.py, ui_context.py | JSON favourites, ordered playlists, stations; atomic temp rename under lock for userdata; shared context revision with weaker write atomicity. |
| Audio | audio_output.py, configure-mpd.sh, apply-audio-output.sh, dac-hats.json | ALSA discovery/routes/mixer, constrained sudo helper, Pi overlays/reboot. PC staging adds pc-alsa and hides HATs. |
| Sources | storage_sources.py, mount-music-drive.sh, udev, storage-refresh unit | USB/internal bind mount/SMB/NFS at /mnt/music, settings-triggered rescan. Device switch detection incomplete at same mount path. |
| Network | network_wifi.py, setup-wifi.sh, wifi-hotspot.sh | NetworkManager owns NIC/AP, cached/locked scan, asynchronous connect and hotspot restoration. Never block offline display on network-online. |
| Receivers | setup-wireless-audio.sh, Bluetooth systemd units | BlueZ/BlueALSA A2DP sink and Shairport Sync ALSA input; optional nqptp. Optional `input_sources` import missing: no verified unified arbitration/metadata. |
| System | library/system.py, server service/control routes, sudoers in install.sh | System info/services/reboot, detached update launch. LAN API unauthenticated, privileged rules need further review. |
| Serving | nginx/pitunes.conf, systemd/pitunes-api.service | Static frontend :80, /api proxy to 127.0.0.1:8080; API as pitunes user, MPD :6600 loopback. Remote interface retained. |
| Startup | setup-kiosk*.sh, display/kiosk-launch, firstboot, systemd/drop-ins | LightDM/Openbox autologin, awake display, no network-online dependency, scan/Samba deferred. Older display unit also remains: do not launch both. |
| Recovery | image/pc/boot/* | X default/fbdev/VESA attempts bounded per boot, LightDM start limits, logs to FAT ESP, BIOS root UUID discovery, compatible graphics GRUB menu. |
| Images | tools/build-flashable-image.sh, chroot-install.sh, image/build.env; build-pc-image.sh + image/pc/*.patch | Pi OS Lite Bookworm ARM vs debootstrap Debian trixie amd64 / bookworm i386. Separate kernels/bootloaders. Reuse mechanics, replace Chromium installation/launch explicitly. |

## Data model and interfaces

Schema 5: artists(id/name/sort_name), albums(title/artist_id/album_artist_id/year/
genre/rating/count/duration/art/revision), tracks(album_id/file_path/title/artist/
composer/disc/track/duration/rating/mtime/size), scan_runs, app_settings and search_fts.
Album identity is application-enforced title+album artist+year; HTTP IDs are numeric
strings. Track IDs are relative file paths, not tracks.id. Do not have Qt write DB.

`/etc/pitunes/userdata.json`: favouriteTracks/favouriteAlbums/playlists/radioStations.
Playlist trackIds preserve order. Browser smart playlists, browse state, font scale
and browser output/repeat/shuffle are separate localStorage. Shared context file
tracks browse/playback revision. Music/credentials are not build inputs.

Complete method/body/response inventory is transfer section 19. Source dispatcher
has GET and POST only, no generic DELETE/PATCH. Important groups:

- Library `/api/library/albums` offset/limit/filter/sort; album/{id}/tracks;
  artists/composers/genres/years/tracks/favourites/starred/tracks/playlists;
  scan-status and `/api/search`. Rescan/cache rebuild are mutations, not repaint.
- Player `/api/player/state`, play/pause/stop/next/previous/seek/volume/options/
  queue/radio/play. State has nested status and MPD-style song fields; body errors
  can appear in HTTP 200. Use exact file and complete album queue, not title-only
  `/api/player/queue` (which still has duplicate-title ambiguity).
- Shared `/api/ui/context`; art/stream ranges; radio directory/stations/icons;
  settings/audio devices/output; filesystem roots/browse; NAS and Wi-Fi/hotspot;
  services/control; system info/control/update status/log/check/apply.
- Legacy `/api/status`, albums/artists/tracks and play-album/play-track/transport
  remain for compatibility. Native uses current player routes.

There is no push event protocol. Async bounded status polling remains necessary;
never poll services or refetch art per animation frame. Timeout/generation checking
must protect UI. Do not retry non-idempotent commands in the Qt client.

## Visual and interaction baseline

Viewed `docs/assets/pitunes-player-gui.png` and inspected renderer/config/events:
black fullscreen, original white logo, prominent square cover, perspective side
stack and vertically faded reflections, centered metadata, circular main transport,
compact bottom Albums/Songs/Artists/Playlists/More/Settings navigation. Active
underline, counts separate from loaded rows, track drawer, focus, mouse wheel,
drag/swipe, keyboard and touch all require parity. Playing identity is independent
of browse index. Base render geometry in renderer.js takes precedence over stale
comments in coverflow.js; 52-degree side angle and .42 s movement are references.

## History and test lessons

Inspected original Git log including queue/continuation (d9892ec), prefix repeat
(4400c8f), release identity (b06b8e5), OTA rollback fixes (324c2d9..8a038d6), awake
display (52adb03/12721cc), PC recovery (4bccf05), software graphics (e08dc50), and
fault test (dd34c46). Earlier reflection revisions were reverted: do not assume a
random historical render commit is the final design.

Original validate workflow checks syntax/update policy. Pi `v*` release workflow
builds both ARM images then marks stable Latest; manual PC workflow builds both
x86 images and gates artifacts on QEMU tests. These were archived inactive here.
PC QEMU tests cover offline BIOS OCR, separate internal disk, correct UEFI bitness,
silent WAV elapsed and forced Xorg failure on a disposable copy. A missing-video
VM was a failed approach because its backend never became reachable.

Retained tests cover album grouping/identity, favourite query scale, userdata bulk
write, repeat/shuffle and prefix queue, browse totals and CSV dedup/formula escape,
version/descriptor policy, updater lock/archive/health rollback and A/B boot guards.
`test_radio_search.py` is an external mock-server probe, not unittest discovery.
No original C++ tests/build existed. Native adds adapter/cache/painting/input tests.

## Migration risks and boundaries

Highest risks: stale request identities, queue resets, loss of browser data,
unbounded decoded art, same-title grouping, dual settings types, missing receiver
manager, Pi mixer/device generation, root helper scope, unauthenticated LAN API,
source updater overwriting native binary, brittle PC patches, unverified base
downloads, and hardware-specific graphics/ALSA behavior. KNOWN_ISSUES tracks each.
No physical native boot, measured RAM saving, bit-perfect output, power-loss-safe
OTA or full feature parity is established by this audit.
