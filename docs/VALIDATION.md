# Native milestone validation

26 November 2026, Apple Silicon macOS. This is development validation, not target
appliance acceptance. Original dd34c46 remains unchanged.

| Executed check | Result |
|---|---|
| Pinned-source comparison | All 174 original tracked blobs match live GitHub; no unpublished code; destination initial snapshot identical |
| Python selected suite | 31 passed, including real HTTP migration/update gates and all four staging paths |
| JavaScript node tests | 24 passed, original identity/counts/CSV/playback contracts |
| CMake Debug + Release | Native arm64 Mac executables compiled successfully with warnings enabled |
| CTest / QTest | 14 substantive cases (16 with init/cleanup) passed in both build configurations |
| Release validator | Shell/Python/JS syntax, App OTA fixtures, signed system fixtures, boot rollback fixtures passed |
| Native image Mac guard | Refused with explicit root Linux/loop-mount requirement, as intended |
| Screenshots | Demo and mock-backed Qt raster output inspected; 800x480 and 480x800 layouts checked |
| Clang built-in analyzer | 10 unique translation units analyzed; three possible std::function closure leak warnings at ApiClient Qt queued/callback ownership sites |

Analyzer findings were reviewed: queued singleShot and QObject::connect functors
are owned by Qt's receiver/connection lifetime and replies use deleteLater. The
analyzer does not model Qt's functor ownership. These are not confirmed leaks, but
the analysis must not be described as warning-free. Lifetime/destruction tests
pass; allocator/leak profiling on target Linux remains open.

Native QTests cover late backend recovery, demo avoiding API/system changes, serialized rapid transport,
mixed state JSON, stable IDs, body errors/invalid JSON, timeout without mutation
retry, origin/size bounds, stale requests, full 5,001-album paging, corrupt image/
LRU budget, <=2 jobs, destroyed receivers, low-budget prefetch stopping at idle,
navigation/reduced animation and zero idle repaint timer activity.

Browser import tests preserve definitions (including unsupported rule fields),
favourites, regular playlists and unknown user-data keys. Re-import deduplicates;
conflicts/corruption refuse without changing disk. Real HTTP tests exercise list,
import, disabled update status and oversized-body rejection before reading it.

No Linux image build, firmware boot, real MPD playback, DAC listening, physical
touch, receiver transition, power loss, constrained-device RAM or performance
comparison was executed on this host. GitHub architecture CI results are recorded
in PROGRESS when observed. Never infer physical acceptance from a package build.
