# Feature parity

Original behavior is the reference. “Retained” means source is inherited, not that
native UI or physical appliance validation is complete. Update individual status
with tests; do not mark entire groups complete after a screenshot.

| Functionality | Original owner / qualification | Native state |
|---|---|---|
| Albums, pagination >5,000, sorting and grouping | queries/scanner, ID+artist+year | Pending native validation |
| Songs and album track drawer | queries, virtual views | Pending |
| Artists / album artists / composers | queries, album filters | Pending |
| Genres / years / album rating filters | queries; no rating editor | Pending |
| Full-text search | SQLite FTS5, fallback | Pending |
| Scan progress, incremental rescan, cache rebuild | scanner/art resolver | Backend retained; native pending |
| Artwork, missing/corrupt fallback | backend 128/420 thumbnails | Backend retained; native pending |
| Classic CoverFlow, reflections, selected focus | Three.js/GSAP, original screenshot | QPainter replacement pending |
| Mouse, wheel, keyboard, touchscreen, drag | web event handlers | Pending |
| Album/song direct playback | ID/file routes, fast start/full prefix | Backend retained; native pending |
| Play / pause / stop / previous / next | MPD API | Backend retained; native pending |
| Seek / volume / duration / elapsed | MPD; volume -1 means no mixer | Backend retained; native pending |
| Repeat Off/All/Song, shuffle | MPD single/repeat/random, radio rejection | Backend/tests retained; native pending |
| Queue and now-playing metadata | MPD, per-track artist/album/year | Backend retained; queue UI pending |
| Automatic next album without UI | idle watcher + backup poller | Retained; native must not duplicate |
| Favourite songs / albums | userdata JSON, optimistic rollback | Backend retained; native pending |
| Playlists create/add/browse/order | JSON trackIds, bulk one-write | Backend retained; native pending |
| Playlist rename/delete/remove/reorder | Absent in original | Requested extension pending |
| Smart rule builder/evaluation | Browser localStorage, some fields absent | Shared storage + explicit migration pending |
| CSV selected lists, dedup, formula escaping | music-export.js exact 23 columns | Remote retained; native export pending |
| Radio search/pagination/save/favourite/remove | Radio Browser proxy/JSON | Retained; native pending |
| Radio MPD playback/errors/metadata | Streams/network dependent | Retained; native pending |
| USB/internal folder source and remount | mount helper/udev | Retained; native picker pending |
| SMB/NFS NAS setup/retry/rescan | NetworkManager/storage backend | Retained; native pending |
| USB DAC, HDMI, analog, Pi I2S HAT | ALSA, overlays; known Pi gaps | Retained; capability UI/hardware pending |
| Device/mixer selection and reboot warning | audio_output + helper | Retained; native pending |
| AirPlay / Bluetooth receiver toggles | services/BlueALSA/Shairport | Retained; native pending |
| Pairing / now-playing / source arbitration | Discoverability exists; missing manager | Existing gap; backend/native work pending |
| Wi-Fi scan/connect, AP recovery, status | NetworkManager worker/cache | Retained; native pending |
| SSH / services, system/about | audited backend helpers | Retained; native pending |
| Reboot / shutdown | delayed backend actions | Retained; native pending |
| Update status/log/progress/rollback | original source updater; Pi A/B gated | Native OTA intentionally disabled |
| Fullscreen, low resolution, focus, touch keyboard | browser UI | Native pending |
| Persistent preferences/browse context | JSON/SQLite/localStorage/context | Migration pending |
| Offline boot, display awake, GUI crash recovery | X11/LightDM/systemd | Native session pending |
| Remote web controls/browser output | frontend/nginx | Source retained at existing paths |
| Four bootable architecture images | Pi OS ARM, Debian x86 | Native integration pending, no hardware boots |
| Branding, logo/favicon/radio art | Original assets | Retained; native resource integration pending |

Physical acceptance includes supported formats/metadata, long playback while
scanning, device removal, no NIC, wrong NAS/Wi-Fi credentials, receiver contention,
no mid-song reset, display awake while paused, actual DAC listening and constrained
RAM. None of these can be established by mock API responses alone.
