# PiTunes Native

![PiTunes](frontend/assets/pitunes-logo-splash.png)

A native C++17 / Qt 5 Widgets evolution of
[PiTunes](https://github.com/BASF2HD/PiTunes), preserving its MPD/ALSA audio engine,
Python API, SQLite library, remote web interface and classic CoverFlow identity.
The local application uses QPainter raster graphics. No embedded browser or
mandatory OpenGL renderer is used.

**Development status:** first native client; full feature parity and hardware
acceptance are in progress. There are no validated native OS downloads or enabled
native OTA updates. Original PiTunes releases are not native images.

## Run on macOS

```sh
brew install cmake ninja qt@5 python@3.12
cmake --preset debug -DCMAKE_PREFIX_PATH="$(brew --prefix qt@5)"
cmake --build --preset debug
ctest --preset debug
python3.12 tools/mock-server.py
# In another terminal, from this repository:
build/debug/pitunes-native --api http://127.0.0.1:8095
```

The inherited mock API supplies demonstration artwork/playback state; it produces
no audio. For an explicitly offline visual demo use
`build/debug/pitunes-native --demo`. API failure is shown as an error and retried;
it never silently substitutes demo playback. Use `--fullscreen`, F11, or
`--reduced-motion`. Python >=3.10 is required; system Python 3.9 is insufficient.

## Runtime boundary

```text
Local Qt Widgets client -> loopback HTTP API -> MPD -> ALSA -> DAC
Remote web browser ----> nginx ------------> same API
                                Python -> SQLite + JSON user data
```

Qt owns presentation; MPD owns audio and queue lifetime. Native code does not
write the library database. `frontend/` remains at its established installed path
for remote browsers. Native devices must never launch Chromium.

The client includes album paging/search, QPainter CoverFlow with perspective and
cached reflections, mouse/wheel/keyboard/touch navigation, track lists, basic
transport/seek/volume/repeat/shuffle, browse filters, favourites, playlist creation,
radio playback, and initial appliance settings. Read the
[parity tracker](docs/FEATURE_PARITY.md) for limitations; a visible control is not
physical hardware validation.

## Targets

| Image | Base | Validation |
|---|---|---|
| i386 | Debian 12 minimal, BIOS / IA32 UEFI | Native image not yet boot-tested |
| amd64 | Debian 13 minimal, BIOS / x64 UEFI | Native image not yet boot-tested |
| armhf | Raspberry Pi OS Lite Bookworm, Zero 2 W / Pi 3 | Native image not yet boot-tested |
| arm64 | Raspberry Pi OS Lite Bookworm, Pi 4 / Pi 5 | Native image not yet boot-tested |

macOS development builds run on the Mac only. Linux packages and disk images
must be built for their own architecture. Image builds are manual development
artifacts, never automatically published stable releases.

## Engineering knowledge

Start with [AGENTS.md](AGENTS.md), [project context](docs/PROJECT_CONTEXT.md),
[architecture audit](docs/ARCHITECTURE_AUDIT.md), [decisions](docs/ARCHITECTURE_DECISIONS.md),
[known issues](docs/KNOWN_ISSUES.md) and [progress](docs/PROGRESS.md).
[The full transfer](PITUNES_KNOWLEDGE_TRANSFER.md) pins the original engineering
reference to `dd34c4635a523b8dabc40e976f2d7b11abf88885`.
The original repository is strictly read-only. No unpublished source was imported.

MIT for PiTunes source; see [LICENSE](LICENSE) and
[third-party notices](THIRD_PARTY_NOTICES.md). Qt retains its applicable license
and distribution/source/relinking obligations.
