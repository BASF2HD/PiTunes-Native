# Native development

Read AGENTS.md and project context first. On Intel or Apple Silicon macOS:

```sh
brew install cmake ninja qt@5 python@3.12
cmake --preset debug -DCMAKE_PREFIX_PATH="$(brew --prefix qt@5)"
cmake --build --preset debug
ctest --preset debug
cmake --preset release -DCMAKE_PREFIX_PATH="$(brew --prefix qt@5)"
cmake --build --preset release
ctest --preset release
```

Homebrew Qt is host architecture only. Tested here: Apple Silicon, AppleClang 21,
Qt 5.15.19, CMake 4.5.1, Ninja 1.13.2, Python 3.12.16. Homebrew Qt 5 is deprecated
upstream; that is not a distro security-support guarantee. Target Linux uses its
distro-maintained Qt 5.15 packages. Record installed versions and review support
before distributing public images.

Mock: `python3.12 tools/mock-server.py`, then
`build/debug/pitunes-native --api http://127.0.0.1:8095`. Mock settings default to
a temporary file; override PITUNES_MOCK_SETTINGS_FILE for isolated fixtures.
The mock does not validate MPD, privileges, mounting, receivers or audio.

Offline demo: `build/debug/pitunes-native --demo`. First paint does not wait for
an API. Development screenshot (create `.cache` first):

```sh
QT_QPA_PLATFORM=offscreen build/debug/pitunes-native --demo \
  --screenshot .cache/native-demo.png
```

Use a real macOS window for manual focus/input checks. Offscreen tests do not
validate a touchscreen or Linux's X11 plugin. Mac binaries are not Linux packages.

Backend tests (explicit selection avoids the standalone network radio probe):

```sh
python3.12 -m unittest tests.test_album_scanner_identity tests.test_music_export_queries \
  tests.test_userdata_playlists tests.test_playback_modes tests.test_system_update \
  tests.test_native_policy tests.test_native_staging tests.test_browser_migration \
  tests.test_native_http -v
node --test tests/*.test.mjs
PITUNES_VALIDATION_PYTHON=python3.12 ./tools/validate-release.sh
```

Use `.clang-format` with clang-format; compilation enables warnings. For static
analysis configure a separate build with `CMAKE_CXX_CLANG_TIDY=clang-tidy` when
installed. Do not claim analysis ran because a config exists. Builds, screenshots,
benchmark runs and images stay ignored.
