# PiTunes Native standalone Windows prototype

The portable build uses the same C++17 application, Qt Widgets, QPainter,
CoverFlow, artwork cache, controls, models and assets. Launch
`pitunes-native.exe`, choose a folder in **Settings → Library → Browse…**, then
select **Save source**. Albums group by folder, using file tags when
available. The prototype plays MP3, FLAC and PCM WAV. It reads common folder
cover images and captures artist, album, title, year, duration and track number
using TagLib. The Windows file picker accepts local folders, attached drives
and Windows-managed shares. **F11** switches window mode; **F12** toggles the
performance overlay. **Settings → Audio** selects the default Windows
device or a detected endpoint. Windows sign-in startup is opt-in.

## Architecture and scope

`native/networking/ApiClient` dispatches the same request contracts in-process
on standalone builds. No HTTP listener, Python, MPD, browser, additional
service, or development environment is needed for local playback. The
Windows edition holds its QSettings and SQLite index in normal per-user
storage, independently of the ZIP's location. Metadata scans run off the UI
thread within the index limit. miniaudio decodes locally and uses WASAPI shared
mode. WAV and FLAC do not undergo lossy re-encoding. Windows still controls the
device mix format, so bit-perfect output is unverified.

Pass `--music-folder <path>` to open a particular library, or use
`--api <url>` for explicitly selecting the existing appliance HTTP backend.
The Linux and Raspberry Pi builds still default to their existing API,
scanner, MPD and ALSA. Their images have no standalone dependency.

The prototype focuses on albums, song browsing/search, and basic playback.
Embedded artwork, playlists, favourites, radio, AAC/ALAC, full artist browsing,
exclusive WASAPI and an installer remain feature-parity work. The monitor
reports Windows CPU, physical RAM, process working set, uptime and actual
CoverFlow frame timing. Windows-specific temperature, per-core and disk
metrics are not supplied by this prototype.

## Portable build and legacy compatibility

The independent workflow builds **PiTunes-Native-Windows-x86.zip** and
**PiTunes-Native-Windows-x64.zip** using the archived Qt 5.15.2 MinGW 8.1
packages and matching compilers/runtimes. The x86 target sets i686/SSE2 and
excludes AVX and AVX2. WASAPI code targets the Win32 6.1 API baseline. Both
ZIPs include Qt platform and SQLite plugins, compiler runtime DLLs and
licence material. The package smoke runs the extracted executable with the
SDK excluded from PATH. A modern runner can validate a portable package and
x86 execution under WOW64. Windows 7 SP1 and audio driver compatibility need
a real legacy boot test.

Use Windows 7 only offline or on an isolated network. Microsoft ended Windows
7 support in January 2020. The pinned legacy Qt distribution also has a
different maintenance status from modern supported builds. Use the Windows
version's filesystem credentials for a trusted network share on supported
machines. No tested legacy installer is supplied.
[Microsoft Windows 7 support FAQ](https://learn.microsoft.com/en-us/troubleshoot/windows-client/windows-7-eos-faq/windows-7-end-support-faq-general)
[Qt 5.15.2 distribution records](https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt5_5152/)
[WASAPI architecture](https://learn.microsoft.com/en-us/windows/win32/coreaudio/wasapi)

## Same-hardware comparison

Use the physical N280/1 GB PC at 1024×600 and 60 Hz. Use the same music
library, artwork, track order and storage medium for Windows standalone,
native Linux and original Chromium PiTunes wherever possible. If Windows
boots from the internal disk and Linux from USB, note both devices and keep
boot and library timings distinct. Run cold and warm library opens separately.
Measure the host's idle baseline before starting the player, then take
60-second steady idle and playback samples and a repeatable sequence of
adjacent-cover moves at the same requested frame limit. Record total available
RAM, process working set or RSS, CPU, dropped frames, paint time, and time
from launch to first cover. Record software or accelerated rendering in
each run. Measure monitor overhead in separate rounds with the monitor off,
then on. Repeat in reverse order to reduce cache and warm-up effects.

The 100 MB application working-set target and the Linux-versus-Windows
comparison remain open until measured on the real Atom.
