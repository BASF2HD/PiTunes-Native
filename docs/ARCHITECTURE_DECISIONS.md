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
| ADR-006 | Visible window of nine covers, byte-budgeted LRU images/reflections, bounded worker pool | 512 MB target cannot retain whole-library full-size art. Network and decode work must not block painting. |
| ADR-007 | Animation only during navigation; 30 Hz transitions, reduced motion option | Time-based easing preserves interaction across CPUs, no idle render loop. Periodic backend state refresh is separate from rendering. |
| ADR-008 | Independent native product/version and disabled OTA | Old source updater cannot install/roll back Qt binaries/dependencies. Neither original feed nor source-only transaction is safe for native devices. |
| ADR-009 | Preserve original disk/boot mechanics; native build profiles and launcher | Retain BIOS UUID recovery, Pi OS firmware, offline mounts and diagnostics. Do not erase internal disks or replace Pi OS with generic Debian. |
| ADR-010 | Shared backend smart playlist storage plus explicit browser export/import | Definitions must survive browser removal. Never inspect a running Chromium profile or silently discard unsupported fields. Original data remains intact. |
| ADR-011 | Keep original release workflows under docs/reference, inactive | Original automatic `v*` Pi release would build Chromium and mark Latest. New CI separates app tests, architecture packages and manual image artifacts; no stable publication. |
| ADR-012 | Separate native GUI recovery from MPD/API | Minimal X11 session, unprivileged GUI, bounded restarts; missing API/network shows a useful shell rather than blocking first paint. |

| ADR-013 | Adapt generated SVG paints in the native artwork worker | Qt 5 SVG Tiny cannot render CSS rgba/patterns. Convert paint to RGB/opacity and omit only the known placeholder dotted overlay, keeping backend/remote output unchanged. |

Do not infer approved native update, image or hardware support from these design
decisions. Every target requires package, boot, input, audio and resource evidence.
