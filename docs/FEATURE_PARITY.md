# Feature parity

Original behavior is the reference. “Retained” means source is inherited, not that
native UI or physical appliance validation is complete. Update individual status
with tests; do not mark entire groups complete after a screenshot.

| Functionality | Original owner / qualification | Native state |
|---|---|---|
| Albums, pagination >5,000, sorting and grouping | queries/scanner, ID+artist+year | Native 200-row paging; 5,001-row contract test passed; hardware pending |
| Songs and album track drawer | queries, virtual views | Native overlay over the centre cover, with numbered virtual rows, singer, duration and row actions. All-songs browse currently first 200 only. |
| Artists / album artists / composers | queries, album filters | Native filter lists implemented; album-artist identity retained |
| Genres / years / album rating filters | queries; no rating editor | Native filters implemented, no rating setter |
| Full-text search | SQLite FTS5, fallback | Native debounced album search; Enter opens matching songs, server limit 120 |
| Scan progress, incremental rescan, cache rebuild | scanner/art resolver | Rescan/rebuild controls; progress presentation pending |
| Artwork, missing/corrupt fallback | backend 128/420 thumbnails | Async bounded native cache/reflections/fallback, decode/LRU/jobs tested |
| Classic CoverFlow, reflections, selected focus | Three.js/GSAP, original screenshot | QPainter fits neighbours to the viewport with an aligned bottom edge and masked reflections. Slide, turn and scale have separate easing. Resize, navigation, reflection and idle tests passed. Exact visual parity pending. |
| Mouse, wheel, keyboard, touchscreen, drag | web event handlers | Native handlers implemented; keyboard QTest passed; physical touch pending |
| Album/song direct playback | ID/file routes, fast start/full prefix | Native submits exact file/full album URI queue; MPD hardware pending |
| Play / pause / stop / previous / next | MPD API | Native async intents, bounded serialization; real HTTP fixture transport tests |
| Seek / volume / duration / elapsed | MPD; volume -1 means no mixer | Native sliders/status implemented; mixed types/no-mixer tested |
| Repeat Off/All/Song, shuffle | MPD single/repeat/random, radio rejection | Native controls; original semantic tests retained; real MPD pending |
| Queue and now-playing metadata | MPD, per-track artist/album/year | One caption below the centre cover shows the playing song, singer and album/year while browsing. Queue editing remains pending. |
| Automatic next album without UI | idle watcher + backup poller | Backend retained; native must not duplicate. Native browse-context publication is still missing, so continuation sort/filter can differ |
| Favourite songs / albums | userdata JSON, optimistic rollback | Basic native save/remove song and save album; album removal/optimistic UI pending |
| Playlists create/add/browse/order | JSON trackIds, bulk one-write | Native create/browse/stored order; add-to-existing native UI pending |
| Playlist rename/delete/remove/reorder | Absent in original | Requested extension pending |
| Smart rule builder/evaluation | Browser localStorage, some fields absent | Shared additive definition import + remote export implemented/tested; native builder/evaluation/playback pending |
| CSV selected lists, dedup, formula escaping | music-export.js exact 23 columns | Remote retained; native export pending |
| Radio search/pagination/save/favourite/remove | Radio Browser proxy/JSON | Retained; native pending |
| Radio MPD playback/errors/metadata | Streams/network dependent | Basic native search/saved-station playback; richer metadata/reconnect pending |
| USB/internal folder source and remount | mount helper/udev | Retained; native picker pending |
| SMB/NFS NAS setup/retry/rescan | NetworkManager/storage backend | Retained; native NAS form implemented, mounting/recovery untested |
| USB DAC, HDMI, analog, Pi I2S HAT | ALSA, overlays; known Pi gaps | Retained; capability UI/hardware pending |
| Device/mixer selection and reboot warning | audio_output + helper | Native async form, hardware apply not tested |
| AirPlay / Bluetooth receiver toggles | services/BlueALSA/Shairport | Native service toggles implemented; actual receivers untested |
| Pairing / now-playing / source arbitration | Discoverability exists; missing manager | Existing gap; backend/native work pending |
| Wi-Fi scan/connect, AP recovery, status | NetworkManager worker/cache | Native form/scan/hotspot controls, hardware handoff pending |
| SSH / services, system/about | audited backend helpers | Native controls, services disabled state needs hardware verification |
| Reboot / shutdown | delayed backend actions | Native confirmation and API request; not executed on hardware |
| Update status/log/progress/rollback | original source updater; Pi A/B gated | Native OTA intentionally disabled |
| Fullscreen, low resolution, focus, touch keyboard | browser UI | Native fullscreen/F11/in-dialog keyboard; 800x480 and 480x800 screenshots checked; complete focus/IME pending |
| Persistent preferences/browse context | JSON/SQLite/localStorage/context | Safe preference import exists; native application/context publishing/following pending |
| Offline boot, display awake, GUI crash recovery | X11/LightDM/systemd | Native unprivileged X11/user service, awake commands and bounded recovery configured; i386/amd64 1 GiB offline/UEFI/failure QEMU tests passed; physical boot pending |
| Remote web controls/browser output | frontend/nginx | Source retained at existing paths |
| Four bootable architecture images | Pi OS ARM, Debian x86 | Four staging paths tested; independent package CI/manual image workflow added; PC VM evidence and current image-build results in PROGRESS; no physical native boots |
| Branding, logo/favicon/radio art | Original assets | Native window icon and remote assets retained. The native player header omits the logo. |

Physical acceptance includes supported formats/metadata, long playback while
scanning, device removal, no NIC, wrong NAS/Wi-Fi credentials, receiver contention,
no mid-song reset, display awake while paused, actual DAC listening and constrained
RAM. None of these can be established by mock API responses alone.
