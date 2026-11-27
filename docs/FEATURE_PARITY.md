# Feature parity

Original behavior is the reference. “Retained” means source is inherited, not that
native UI or physical appliance validation is complete. Update individual status
with tests; do not mark entire groups complete after a screenshot.

| Functionality | Original owner / qualification | Native state |
|---|---|---|
| Albums, pagination >5,000, sorting and grouping | queries/scanner, ID+artist+year | Native 200-row paging and a translucent album transport strip with arrows, click-to-jump and drag. The Albums menu changes scope and sort independently. Idle browse returns to a playing album present in the current results after five seconds. |
| Songs and album track drawer | queries, virtual views | Square frosted overlay with numbered virtual rows, track singer/artist, pulse indicator and row actions. The scanner recovers embedded M4A track credits on rescan without changing album IDs. The Songs menu switches CoverFlow to paged per-track covers, favourite songs, or album groups. Two-finger scrolling stays in the drawer and dragging across a title selects its text. |
| Artists / album artists / composers | queries, album filters | Native filter lists implemented; album-artist identity retained |
| Genres / years / album rating filters | queries; no rating editor | Native filters implemented, no rating setter |
| Full-text search | SQLite FTS5, fallback | Floating search panel and touch keyboard preserve cover and transport geometry. Debounced library search shows matching song and album covers using the server’s result limits. Closing restores the prior album scope and sort. |
| Scan progress, incremental rescan, cache rebuild | scanner/art resolver | Rescan/rebuild controls; progress presentation pending |
| Artwork, missing/corrupt fallback | backend 128/420 thumbnails | Async bounded native cache/reflections/fallback, decode/LRU/jobs tested |
| Classic CoverFlow, reflections, selected focus | Three.js/GSAP, original screenshot | QPainter projects borderless cover art and reflections from a shared floor. The earlier native 36-degree stack projection is paired with the NaviGlassPlayer-style retargeted glide for regular and fast scroll. Artwork remains visible behind the footer and frosted popups. |
| Mouse, wheel, keyboard, touchscreen, drag | web event handlers | Native handlers implemented; keyboard QTest passed; physical touch pending |
| Album/song direct playback | ID/file routes, fast start/full prefix | Native submits exact file/full album URI queue; MPD hardware pending |
| Play / pause / stop / previous / next | MPD API | Circular play/pause and distinct track skip buttons below the covers. Manual Next can advance to the next album in the original playback order. The backend still exposes Stop. Native async intents and bounded serialization tested against an HTTP fixture. |
| Seek / volume / duration / elapsed | MPD; volume -1 means no mixer | Compact playback row with a 480 px maximum seek ruler supports direct click-to-seek and drag. The volume popup toggles mute and restores the previous level. API payloads and reported state tested. |
| Repeat Off/All/Song, shuffle | MPD single/repeat/random, radio rejection | Icon controls in the top playback bar mirror reported state. Original semantic tests retained. Real MPD pending. |
| Queue and now-playing metadata | MPD, per-track artist/album/year | The caption follows the centred album with album name and artist/year. On the playing album it shows song, singer and `Album \| Year`. Queue editing remains pending. |
| Album and track details | NaviGlassPlayer metadata panels | Native More info shows selectable fields, including album totals, composer, singer, file path, codec and reported bitrate. Album tracks hydrate the panel by ID and relative URI. Missing tags are marked Unknown. |
| Automatic next album without UI | idle watcher + backup poller | Backend retained; native sends the album sort/filter with playback. Automatic continuation and manual Next at the end of an album share backend ordering |
| Favourite songs / albums | userdata JSON, optimistic rollback | White album and inline song hearts load saved state, toggle both ways and roll back failed saves. Song hearts sit just before the duration. |
| Playlists create/add/browse/order | JSON trackIds, bulk one-write | Native create, browse and sort, with the selected playlist shown as track covers. The row and caption menus add to existing playlists using ordered single-track calls and stop on the first failure. |
| Playlist rename/delete/remove/reorder | Absent in original | Requested extension pending |
| Smart rule builder/evaluation | Browser localStorage, some fields absent | Shared additive definition import + remote export implemented/tested; native builder/evaluation/playback pending |
| CSV selected lists, dedup, formula escaping | music-export.js exact 23 columns | Remote retained; native export pending |
| Radio search/pagination/save/favourite/remove | Radio Browser proxy/JSON | More → Radio shows saved station art on the CoverFlow shelf, with double-click or station actions for playback. Find internet radio searches and pages the directory and saves results. Favourite and remove controls remain pending. |
| Radio MPD playback/errors/metadata | Streams/network dependent | Play by saved station ID or directory stream URL. Richer metadata and reconnect handling remain pending. |
| USB/internal folder source and remount | mount helper/udev | Native Settings folder picker browses readable directories on the API host, including locally mounted storage. Device lifecycle validation is pending. |
| SMB/NFS NAS setup/retry/rescan | NetworkManager/storage backend | Retained; native NAS form implemented, mounting/recovery untested |
| USB DAC, HDMI, analog, Pi I2S HAT | ALSA, overlays; known Pi gaps | Retained; capability UI/hardware pending |
| Device/mixer selection and reboot warning | audio_output + helper | Native async appliance form. The desktop preview uses the System sound output label for host-device selection where supported. On macOS, CoreAudio provides the device list. Appliance routes remain available. Linux hardware apply remains untested. |
| AirPlay / Bluetooth receiver toggles | services/BlueALSA/Shairport | Native service toggles implemented; actual receivers untested |
| Pairing / now-playing / source arbitration | Discoverability exists; missing manager | Existing gap; backend/native work pending |
| Wi-Fi scan/connect, AP recovery, status | NetworkManager worker/cache | Native form/scan/hotspot controls, hardware handoff pending |
| SSH / services, system/about | audited backend helpers | Native controls, services disabled state needs hardware verification |
| Reboot / shutdown | delayed backend actions | Native confirmation and API request; not executed on hardware |
| Update status/log/progress/rollback | original source updater; Pi A/B gated | Native OTA intentionally disabled |
| Fullscreen, low resolution, focus, touch keyboard | browser UI | Native fullscreen button/F11 and in-dialog keyboard; 800x480 and 480x800 screenshots checked; complete focus/IME pending |
| Persistent preferences/browse context | JSON/SQLite/localStorage/context | Local caption preferences persist independently for title, singer and album (10–80 px, default 40/25/25). Safe preference import exists. Native browse context publishing/following pending. |
| Offline boot, display awake, GUI crash recovery | X11/LightDM/systemd | Native unprivileged X11/user service, awake commands and bounded recovery configured; i386/amd64 1 GiB offline/UEFI/failure QEMU tests passed; physical boot pending |
| Remote web controls/browser output | frontend/nginx | Source retained at existing paths |
| Four bootable architecture images | Pi OS ARM, Debian x86 | Four staging paths tested; independent package CI/manual image workflow added; PC VM evidence and current image-build results in PROGRESS; no physical native boots |
| Branding, logo/favicon/radio art | Original assets | Native window icon and remote assets retained. The native player header omits the logo. |

Physical acceptance includes supported formats/metadata, long playback while
scanning, device removal, no NIC, wrong NAS/Wi-Fi credentials, receiver contention,
no mid-song reset, display awake while paused, actual DAC listening and constrained
RAM. None of these can be established by mock API responses alone.
