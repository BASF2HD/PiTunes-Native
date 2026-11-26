# Native milestone validation

26 November 2026, Apple Silicon macOS. This is development validation, not target
appliance acceptance. Original dd34c46 remains unchanged.

| Executed check | Result |
|---|---|
| Pinned-source comparison | All 174 original tracked blobs match live GitHub; no unpublished code; destination initial snapshot identical |
| Python selected suite | 32 passed, including real HTTP migration/update gates and all four staging paths |
| JavaScript node tests | 24 passed, original identity/counts/CSV/playback contracts |
| CMake Debug + Release | Native arm64 Mac executables compiled successfully with warnings enabled |
| CTest / QTest | 17 substantive cases (19 with init/cleanup) passed in both build configurations |
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
navigation/reduced animation, SVG rgba colour/alpha compatibility and zero idle repaint timer activity.

Browser import tests preserve definitions (including unsupported rule fields),
favourites, regular playlists and unknown user-data keys. Re-import deduplicates;
conflicts/corruption refuse without changing disk. Real HTTP tests exercise list,
import, disabled update status and oversized-body rejection before reading it.

GitHub-hosted CI built/tested/installed all four architecture packages and rendered
software PNGs at bc6b201 ([run 45905654870](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45905654870)).
Later SVG compatibility changes have passed the local suites; their CI is tracked
in PROGRESS. Generated image candidates remain at e428678, before those UI fixes.

The amd64 disk image passed 1 GiB QEMU BIOS offline GUI/OCR and x64 UEFI GUI/MPD
silent-WAV elapsed playback. The enhanced failure test observed bounded failed
LightDM state, continuing MPD playback, and extracted the FAT diagnostic report
([run 45905118978](https://github.com/BASF2HD/PiTunes-Native/actions/runs/45905118978)).
VM screenshots were inspected. This is no physical firmware, DAC listening,
touch, receiver transition, power-loss or target performance acceptance. See
PROGRESS for the other image and VM test results. No physical native boots exist.
