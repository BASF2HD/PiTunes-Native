# Architecture decisions

Status and rationale, 26 November 2026. Historical choices are distinguished from
native implementation work. Implementation state is recorded in PROGRESS.

| ID | Decision | Rationale / consequence |
|---|---|---|
| ADR-001 | Pin inherited source to dd34c4635a523b8dabc40e976f2d7b11abf88885; read-only original | Prevent regression to old public preview; record all adaptations separately. |
| ADR-002 | Retain Python + MPD + ALSA + SQLite and JSON stores | Proven functionality survives display replacement. Qt is a client; GUI restarts cannot own audio lifetime. |
| ADR-003 | C++17, Qt 5 Widgets/Network/Concurrent, QPainter, CMake | Software raster painting works without WebGL; Qt 5 exists in target distro package sets. Host Qt 5 upstream deprecation means distro security support and later Qt migration need ongoing review. |
| ADR-004 | Keep `frontend/` at its existing installed location | Preserve nginx paths, remote playback and backend compatibility; naming alone is not worth breaking contracts. No embedded browser. |
| ADR-005 | API response adapter, request timeouts, stale-generation guards | Mixed schemas and async races were historical defects. Do not retry mutations; backend already has ambiguous MPD retries. |
| ADR-006 | Size the visible cover window to the viewport and artwork budget | A wider viewport can show more neighbours. The byte-budgeted LRU and two workers bound memory and decode work on small devices. |
| ADR-007 | Retarget cover poses on the earlier native perspective rail | Screen position uses the NaviGlassPlayer-style 420 ms glide. Rotation and depth settle together over 280 ms, after which the flat cover continues translating. The 36-degree side pose and reflection geometry match the earlier native renderer. Wheel bursts retarget the current poses while a single frame timer keeps painting. Both ends of the move use bounded cover windows. The timer runs at 16 ms on macOS and 33 ms on Linux and stops at rest. HiDPI transitions use a bounded logical-resolution surface. Reduced motion snaps to the target. |
| ADR-008 | Independent native product/version and disabled OTA | Old source updater cannot install/roll back Qt binaries/dependencies. Neither original feed nor source-only transaction is safe for native devices. |
| ADR-009 | Preserve original disk/boot mechanics; native build profiles and launcher | Retain BIOS UUID recovery, Pi OS firmware, offline mounts and diagnostics. Do not erase internal disks or replace Pi OS with generic Debian. |
| ADR-010 | Shared backend smart playlist storage plus explicit browser export/import | Definitions must survive browser removal. Never inspect a running Chromium profile or silently discard unsupported fields. Original data remains intact. |
| ADR-011 | Keep original release workflows under docs/reference, inactive | Original automatic `v*` Pi release would build Chromium and mark Latest. New CI separates app tests, architecture packages and manual image artifacts; no stable publication. |
| ADR-012 | Separate native GUI recovery from MPD/API | Minimal X11 session, unprivileged GUI, bounded restarts; missing API/network shows a useful shell rather than blocking first paint. |

| ADR-013 | Adapt generated SVG paints in the native artwork worker | Qt 5 SVG Tiny cannot render CSS rgba/patterns. Convert paint to RGB/opacity and omit only the known placeholder dotted overlay, keeping backend/remote output unchanged. |
| ADR-014 | Project the cover face and its reflection onto a shared floor | A projection makes both foot edges and reflection widths follow the same turn. Mask reflections by cover order, then paint the faces so a reflection cannot erase its neighbour's artwork. |
| ADR-015 | Anchor native overlays to the cover stage | Search, its touch keyboard, the caption actions and transport stay out of the cover layout. The album strip is composited over the reflections. Footer menus and the drawer suspend the five-second idle return to the playing album. |
| ADR-016 | Frost player popups with a bounded QPainter backdrop | The drawer, search, details and menus sample artwork and reflections at reduced resolution and reuse a blurred image until their position or the underlying artwork changes. The effect works without a desktop compositor or continuous repaint timer. |
| ADR-017 | Store caption typography as a local display preference | The Font tab sets title, artist or singer, and album pixel sizes independently. The Qt stage reserves space for all three and fits them on smaller displays. The preferences use QSettings and stay separate from appliance data. |
| ADR-018 | Use platform-neutral system output labels | Settings checks the API host before using macOS CoreAudio controls. MPD keeps playing through the system default output. The explicit output action changes the Mac default and requires no appliance route change. The preview also displays the appliance routes and device catalogue. Applying those routes remains available when connected to an appliance. |

| ADR-019 | Reuse the inherited radio directory, icon proxy and station store | The native dialog separates play from save. It requests artwork for visible rows through the existing API with a short cold-cache wait, decodes 64 px thumbnails asynchronously, and shows initials when a logo is unavailable. Existing remote clients keep the immediate fallback. |

Do not infer approved native update, image or hardware support from these design
decisions. Every target requires package, boot, input, audio and resource evidence.
