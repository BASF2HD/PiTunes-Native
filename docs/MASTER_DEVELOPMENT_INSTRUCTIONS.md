# MASTER DEVELOPMENT INSTRUCTION — PiTunes Native OS

## 1. Project identity and mission

You are the lead systems architect, senior C++/Qt developer, Linux distribution engineer, embedded systems engineer, build/release engineer and QA engineer for a new open-source project:

**Project:** PiTunes Native
**New GitHub repository:** https://github.com/BASF2HD/PiTunes-Native
**Original reference repository:** https://github.com/BASF2HD/PiTunes
**Local workspace:** The existing `PiTunes-Native` folder currently opened in Codex on my Mac
**Licence:** MIT, preserving required existing third-party notices and attribution.

Your mission is to develop a genuinely native, lightweight, bootable PiTunes music player OS for legacy Intel/AMD PCs and Raspberry Pi devices.

The primary architectural change is replacing the existing Chromium, HTML/CSS/JavaScript and Three.js/WebGL local kiosk with a native **C++17 + Qt Widgets + QPainter** application.

This is NOT a request for a proposal alone. You must implement the project, write production-quality code, create the build infrastructure, run available tests, commit your changes and push the new project to `BASF2HD/PiTunes-Native`.

Proceed autonomously through tasks that can be completed safely. Do not repeatedly ask for decisions already specified in this brief. If a task requires hardware or permissions that are unavailable, document the blocker accurately and continue with independent tasks. Never claim a test passed unless it was actually run.

## 2. Repository and workspace isolation — critical

The original `BASF2HD/PiTunes` repository contains the existing working Chromium-based application and published releases.

It is a read-only reference for this project.

STRICT RULES:

- Work exclusively within the current `PiTunes-Native` workspace.
- Do not edit, commit, push, tag or publish anything in `BASF2HD/PiTunes`.
- Inspect the current directory and Git repository before modifying anything.
- Verify the destination Git remote is exactly `https://github.com/BASF2HD/PiTunes-Native.git` or the corresponding SSH URL.
- Clone the existing new repository into the workspace if appropriate, but handle the fact that the folder already exists and may be empty.
- Never initialise a nested Git repository accidentally.
- Never overwrite an existing populated folder without inspecting it.
- Do not force-push or use destructive Git commands.
- Keep source files, compiled binaries, generated OS images and temporary build artefacts properly separated.
- Preserve the full project history of the new repository.

First, execute and inspect the equivalent of:

`pwd`

`git status`

`git remote -v`

`git branch --show-current`

If this local directory is not yet connected to the new repository, initialise or clone it safely and set up the correct remote.

Inspect both repositories before deciding which files need to be migrated. Use the original PiTunes repository at a pinned commit as the starting reference.

## 3. Thorough original application audit

Before implementing the native interface, examine the entire original PiTunes codebase.

Audit at least:

- `README.md`
- `backend/`
- `backend/library/`
- `frontend/`
- `config/`
- `scripts/`
- `systemd/`
- `nginx/`
- `image/`
- `tools/`
- `tests/`
- `install.sh`
- `configure-mpd.sh`
- Existing Raspberry Pi image builder
- Existing Intel/AMD PC image builder
- Current GitHub Actions workflows
- Existing OTA, recovery, rollback and deployment architecture

Understand the actual implementation rather than relying only on README descriptions.

Create `docs/ARCHITECTURE_AUDIT.md` describing the complete architecture, dependencies, service responsibilities, APIs, data models, feature inventory and potential migration risks.

Create `docs/FEATURE_PARITY.md` containing a complete checklist of every existing user-visible feature.

Inspect the original CoverFlow screenshots, artwork, layout, animations, menus and controls to establish a visual baseline.

Preserve the existing PiTunes identity and branding. Do not invent replacement logos or substantially redesign the appearance.

## 4. Operating system architecture

Create a unified PiTunes Native application using four architecture-specific bootable OS images.

### Target A — Intel/AMD 32-bit

- Architecture: i386/i686
- Base: Debian 12 minimal
- Target: Intel Atom computers with 1 GB RAM
- Legacy BIOS support
- 32-bit UEFI where supported
- Check actual CPU PAE/SSE and other instruction-set requirements.
- Do not generate binaries incompatible with the documented minimum CPU.

### Target B — Intel/AMD 64-bit

- Architecture: amd64
- Base: the existing supported Debian PC base, initially Debian 13
- Legacy BIOS and compatible UEFI
- Modern Intel/AMD systems

### Target C — Raspberry Pi 32-bit

- Architecture: armhf
- Base: Raspberry Pi OS Lite Bookworm
- Devices: Raspberry Pi Zero 2 W, Pi 3 and Pi 3 B+
- Primary constrained target: Pi Zero 2 W with 512 MB RAM

### Target D — Raspberry Pi 64-bit

- Architecture: arm64
- Base: Raspberry Pi OS Lite Bookworm
- Devices: Raspberry Pi 4 and Pi 5

Do not replace Raspberry Pi OS with generic Debian merely for consistency.

Use the appropriate Raspberry Pi firmware, kernel, boot partition, overlays and hardware configuration.

All four targets must share the same native application source code wherever practical.

Each target must have its own reproducible build configuration and bootable image.

Do not assume that a single binary or disk image will run across these different CPU architectures.

Use stable, security-supported dependencies and pin verified versions.

## 5. Native interface — mandatory architecture

Implement the local PiTunes GUI as a genuinely native C++17 application.

Required framework:

- Qt 5 Widgets initially, subject to verified support on all targets
- QPainter-based custom graphics
- QImage/QPixmap handling
- QNetworkAccessManager for API communication
- Qt signals and slots
- Event-driven UI updates
- CMake for compiling the application

Do not use:

- Chromium
- Electron
- Qt WebEngine
- Qt WebView
- CEF
- Embedded browser widgets
- HTML-based native UI
- JavaScript-based CoverFlow
- Mandatory OpenGL/WebGL acceleration

The native interface must not be a wrapper around the existing web application.

Implement custom-painted widgets where necessary to faithfully reproduce the PiTunes design.

Make the renderer efficient on older Intel Atom graphics and Raspberry Pi hardware.

Provide a software-rendering path without requiring functional GPU acceleration.

Avoid unnecessary composition effects, expensive full-screen repaints, continuous animation loops or unbounded memory allocation.

Use Qt's event system rather than CPU-intensive polling.

## 6. Native CoverFlow graphics

Recreate the original PiTunes CoverFlow appearance as faithfully as reasonably possible.

Required capabilities:

- Centred active album cover
- Side album covers with perspective-like transformations
- Smooth left/right navigation
- Cover scaling and positioning
- Cover reflections where performance permits
- Background and visual styling closely matching the original
- Album title, artist and year
- Keyboard, mouse and touchscreen navigation
- Mouse wheel and touch gesture support
- Smooth selection transitions
- Focus and selection state
- Now-playing indicators
- Opening an album's track list
- Direct album and song playback

Use QPainter transforms and preprocessed imagery where appropriate.

Optimisations:

- Render only visible album covers.
- Start with 7–9 visible covers on low-memory devices.
- Maintain a bounded prefetch window.
- Cache scaled artwork at appropriate target dimensions.
- Decode image files asynchronously.
- Avoid decoding full-resolution images unnecessarily.
- Avoid disk I/O on the GUI thread.
- Avoid repeated reflection generation.
- Avoid repainting unchanged screen regions.
- Stop animation updates completely when idle.
- Support reduced animations when hardware performance is insufficient.
- Use time-based animation to maintain consistency across different CPUs.
- Handle missing, corrupted and unsupported artwork gracefully.

Ensure the artwork cache has a strictly bounded memory budget and sensible eviction policy.

Never retain thousands of full-resolution album images in RAM.

Support libraries exceeding 5,000 albums.

## 7. Full feature parity

Do not create a simplified player that removes existing PiTunes functionality.

Reimplement the complete local user interface while retaining the proven underlying features.

### Music library

- Albums
- Songs
- Artists
- Album artists
- Composers
- Genres
- Years
- Album ratings where supported
- Full-text search
- Album track listings
- Album grouping and sorting
- Large library pagination
- Incremental library scanning
- Scan progress and status
- Artwork caching and rebuilding

### Playback

- Play
- Pause
- Stop
- Previous
- Next
- Seek
- Volume
- Repeat Off / All / Song
- Shuffle
- Playback queue
- Now playing
- Song duration and elapsed time
- Track artist, album and year
- Supported playback formats and metadata

### User library features

- Favourite songs
- Favourite albums
- Create and manage playlists
- Smart playlists
- Playlist browsing and editing
- Export music lists to CSV

Review the original smart-playlist and browser-local-storage implementation carefully. Move data that must persist independently of a browser into an appropriate shared storage model, with migration and compatibility tests. Do not silently discard existing user data.

### Internet radio

- Radio station search
- Station favourites
- Playback through MPD
- Radio metadata where available
- Error and reconnection handling

### Music sources

- USB storage
- Internal music folder
- NAS SMB
- NAS NFS
- Automatic mount and remount
- Library rescan when music sources change

### Audio output

- USB DAC
- HDMI audio
- Analogue audio
- Supported Raspberry Pi I2S DAC HATs
- ALSA device selection
- Output configuration
- Appropriate hardware/software mixer handling
- Preserve bit-perfect or otherwise transparent lossless playback where supported by the original audio pipeline and selected hardware.

### Wireless services

- AirPlay receiver
- Bluetooth A2DP receiver
- Bluetooth pairing
- Receiver enable/disable
- Appropriate now-playing status

### Network and system

- WiFi scanning
- WiFi connection
- Recovery hotspot
- Network status
- SSH service control
- System settings
- Storage configuration
- Software version and update status
- Reboot
- Shutdown
- Service control

### Interface requirements

- Fullscreen appliance-style player
- No visible Linux desktop
- Touchscreen-friendly controls
- On-screen keyboard where necessary
- Automatic screen resolution detection
- Graceful low-resolution layouts
- Persistent user preferences
- Proper focus and navigation
- Consistent design with original PiTunes

Preserve existing behaviour wherever feasible and document any intentionally changed behaviour.

## 8. Reuse the existing backend

Do not rewrite functioning backend code unnecessarily.

Reuse and adapt the existing:

- MPD music playback engine
- ALSA output configuration
- Python API
- SQLite music database
- Album artwork resolver
- Library scanner
- Playlist and favourites storage
- Internet radio integration
- Network configuration
- Music source mounting
- Existing helper scripts and services

The new local C++ interface should communicate with the existing Python API through well-defined interfaces.

Prefer a local loopback connection or Unix-domain socket, subject to the existing architecture.

Keep the existing remote web interface available through nginx for access from another computer, smartphone or tablet.

Important distinction:

Chromium must not be required or launched on the PiTunes Native device.

The original remote web interface may continue to be served as static HTML, CSS and JavaScript. It runs in the remote user's browser, not in a browser process on the music player.

Document and test the boundary between the local C++ GUI, backend API and remote web interface.

Do not introduce a second independent music database or separate playback state.

## 9. Low-memory and Intel Atom optimisation

The Intel Atom 1 GB computer is a priority development target.

The Raspberry Pi Zero 2 W with 512 MB RAM is equally important.

Analyse and optimise:

- Linux boot services
- Display server memory
- Native application memory
- Python API memory
- MPD memory
- SQLite access
- Artwork cache
- Network and wireless services
- Disk I/O
- Music library scanning
- Background tasks
- Graphics rendering
- CPU wakeups and polling
- Memory fragmentation

Use minimal X11 initially, with a lightweight kiosk launcher and no full desktop environment.

Evaluate framebuffer or DRM/KMS-based display integration later only when it offers measurable improvements and reliable hardware compatibility.

Configure zram only where appropriate and benchmark the effect. Do not blindly assume zram improves every workload.

Disable optional services by default on low-resource devices only where doing so preserves the required appliance behaviour and user controls.

Avoid unnecessary network waits during boot.

Keep playback responsive while the library is scanning.

Prioritise startup reliability and fast response over decorative visual effects.

## 10. Performance targets

Treat these as engineering targets, NOT guaranteed outcomes.

### Intel Atom, 1 GB RAM

- Low idle RAM consumption
- Aim for 200–350 MB total system RAM where achievable
- Aim for under 500 MB during normal playback
- Smooth and responsive CoverFlow
- Avoid sustained high CPU utilisation when idle
- No unwanted disk swapping
- Fast startup from supported storage

### Raspberry Pi Zero 2 W, 512 MB RAM

- Operate reliably within 512 MB physical RAM
- Use significantly smaller artwork caches
- Avoid out-of-memory failures
- Preserve playback under background load
- Use reduced animations automatically if necessary

### General

- Aim for smooth 20–30 FPS transitions on constrained hardware
- No continuous rendering when idle
- Keep normal music playback responsive
- Minimise the time between OS boot and a usable player interface
- Preserve audio fidelity

Measure actual performance rather than using assumptions.

## 11. Baseline and performance comparison

A major purpose of this project is comparing Chromium-based PiTunes with native C++ PiTunes.

Create a reproducible benchmarking methodology.

Measure:

- Power-on to usable interface
- Operating system memory
- Application memory
- Total memory during playback
- Idle CPU utilisation
- CPU during CoverFlow navigation
- UI frame rate and frame-time distribution
- Artwork load latency
- Library search latency
- Library scan impact
- Disk I/O
- Swap usage
- Audio underruns
- Stability during extended playback

Use identical hardware, storage, display resolution, library data and relevant background services for comparisons.

Distinguish cached and cold-start measurements.

Create benchmark scripts and store results in `docs/benchmarks/`.

Do not invent numbers.

Mark results clearly as:

- Physical hardware measurement
- Virtual machine measurement
- Static analysis
- Estimated engineering target
- Not yet tested

If only emulation is available, report it honestly.

## 12. Mac development environment

My main development machine is a Mac running Codex.

The active workspace is the newly created `PiTunes-Native` folder.

Create a practical local development workflow suitable for macOS.

Requirements:

- CMake project
- Clear dependency setup
- Qt Widgets development environment
- Native GUI development and preview on macOS where feasible
- Mock API or test fixtures for UI development
- Automated unit tests
- Debug and release build configurations
- Source formatting
- Static analysis
- Architecture documentation

Use a native macOS build for development and UI validation where possible.

Do not pretend that a macOS-compiled binary will run directly on Linux or Raspberry Pi.

Use an appropriate Debian/Ubuntu Linux build environment, container or virtual machine for Linux compilation and OS image generation.

Apple Silicon and Intel Macs must be considered where toolchain behaviour differs.

Cross-compilation, chroot and QEMU emulation must be configured accurately for the target architecture.

Do not require me to maintain multiple manually configured development machines unless genuinely necessary.

## 13. Independent OS image builders

Create build infrastructure for:

`pitunes-native-i386.img.xz`

`pitunes-native-amd64.img.xz`

`pitunes-native-armhf.img.xz`

`pitunes-native-arm64.img.xz`

Each image must include:

- Appropriate Linux base OS
- Architecture-specific kernel and boot configuration
- Native PiTunes C++ application
- Existing adapted PiTunes backend
- MPD and ALSA
- SQLite
- Required network and storage services
- Minimal display environment
- Automatic native player startup
- First-boot configuration
- Appropriate hardware support
- Recovery and diagnostic tools

Do not install Chromium, Qt WebEngine, an unnecessary desktop environment or browser-related dependencies into the native image.

Use minimum necessary Linux packages.

Maintain separate build outputs and architecture-specific configuration.

For Intel/AMD, retain bootable USB behaviour without modifying the existing internal disk.

For Raspberry Pi, retain SD-card flashing using Raspberry Pi Imager or an equivalent compatible writer.

Ensure first boot can operate offline with locally attached music.

Do not distribute a generic x86 image as a Raspberry Pi image or vice versa.

## 14. Boot, recovery and reliability

The device should boot directly into PiTunes Native.

Implement and test:

- Automatic kiosk launch
- Splash/startup indication if appropriate
- Native app crash recovery with rate limiting
- Safe shutdown
- Readable application logs
- Boot diagnostics
- Display driver fallback for older Intel hardware
- Graceful behaviour when graphics acceleration is unavailable
- Audio service independence from GUI crashes
- Recovery when music storage is missing
- Correct handling of unavailable network services
- Safe persistence of settings and library data

A GUI crash must not corrupt the music library, settings or playlist database.

Avoid uncontrolled restart loops.

Do not disable important system power management or security features merely to suppress boot errors.

## 15. Updates and release isolation

The native application must have its own release and update identity.

Repository:

`BASF2HD/PiTunes-Native`

Do not use the original PiTunes release feed for native application updates.

Do not push native OTA packages to the original PiTunes project.

Do not allow the existing PiTunes Chromium installations to accidentally update to incompatible native packages.

Design a separate update manifest, package identity and release channel.

Reuse verified rollback mechanisms where practical, but adapt them to the new architecture and native application packaging.

Keep update support disabled until it is independently tested.

Do not overwrite user music, favourites, playlists or settings during an update.

## 16. Security and public repository requirements

This repository is public.

Never commit:

- API tokens
- GitHub personal access tokens
- SSH private keys
- Passwords
- WiFi credentials
- NAS usernames or passwords
- Real user configuration secrets
- Private signing keys
- Personal files
- Unnecessary binary build outputs

Audit imported files for secrets before committing.

Preserve required licences and third-party notices.

Use least-privilege services and appropriate filesystem permissions.

Avoid unnecessary root execution by the native GUI.

Restrict privileged system actions through audited backend helpers.

Avoid unsafe shell command construction, path traversal and unrestricted network shares.

Document any first-boot credential or network security risks.

## 17. GitHub Actions and build performance

Create independent GitHub Actions workflows in the new repository.

Requirements:

- C++ compilation and unit tests
- Backend tests
- Integration tests
- Architecture-specific build validation
- Bootable image generation
- Image integrity checks
- SHA-256 checksums
- Optional emulator smoke testing
- Manually triggered image builds
- Separate prerelease publishing
- Release validation

Avoid rebuilding entire OS images for every small C++ change.

Use a staged build system:

1. Fast C++ application compilation and unit tests
2. Backend/integration tests
3. Target-architecture application package builds
4. OS image generation only when requested or required for a release

Use package caching, compiler caching and dependency caching where useful.

Avoid unnecessary Docker image rebuilding and repeated dependency downloads.

Use GitHub-hosted and self-hosted runners only where correctly configured.

Do not assume a macOS GitHub runner can directly produce Raspberry Pi boot images without suitable tooling.

Ensure workflows cannot publish to the original PiTunes repository.

Do not automatically publish release images merely because a development commit was pushed.

## 18. Project structure

Create a maintainable repository structure, adapting it based on the actual architecture audit.

Suggested organisation:

PiTunes-Native/
- AGENTS.md
- README.md
- LICENSE
- CHANGELOG.md
- CMakeLists.txt
- native/
  - app/
  - ui/
  - coverflow/
  - artwork/
  - networking/
  - models/
  - settings/
  - assets/
- backend/
- frontend-remote/
- config/
- systemd/
- scripts/
- image/
  - pc/
  - raspberry-pi/
  - common/
  - out/
- tools/
- tests/
  - native/
  - backend/
  - integration/
  - boot/
- docs/
  - ARCHITECTURE_AUDIT.md
  - NATIVE_ARCHITECTURE.md
  - FEATURE_PARITY.md
  - DEVELOPMENT.md
  - BUILDING.md
  - IMAGE_CREATION.md
  - PERFORMANCE.md
  - HARDWARE_COMPATIBILITY.md
  - benchmarks/
- .github/workflows/

This is a proposed structure, not a requirement to create empty placeholder directories.

Use actual meaningful source files and documentation.

Do not duplicate entire backend implementations unnecessarily.

## 19. AGENTS.md — persistent Codex instructions

Create a comprehensive root `AGENTS.md` that establishes permanent project rules.

The file must instruct future Codex sessions to:

- Work only in the PiTunes-Native repository.
- Verify the Git remote before pushing.
- Never modify the original PiTunes repository.
- Use C++ and genuine Qt Widgets for the local interface.
- Never reintroduce Chromium or Qt WebEngine.
- Preserve existing feature parity.
- Keep the backend and remote web interface compatible.
- Support all four target architectures.
- Prioritise 1 GB Atom and 512 MB Raspberry Pi hardware.
- Validate before claiming success.
- Use small, testable commits.
- Avoid unnecessary complete OS rebuilds.
- Maintain documentation and progress tracking.
- Record actual benchmark results.
- Avoid exposing secrets.
- Never publish unvalidated production images.

Also create `docs/PROGRESS.md`, documenting completed work, current limitations, next tasks and validation results so future Codex conversations can resume without losing context.

## 20. Implementation strategy

Do not spend the entire task only analysing the codebase.

Perform actual implementation in logical stages.

### Stage 1 — Foundation

- Verify the workspace and Git remote.
- Inspect the original project.
- Import reusable source code safely.
- Preserve licence requirements.
- Create AGENTS.md.
- Create the architecture audit.
- Establish CMake and native application structure.
- Establish local macOS development instructions.

### Stage 2 — Native prototype

Build a working native PiTunes player with:

- Fullscreen Qt Widgets main window
- Album artwork
- 2D CoverFlow
- Smooth navigation
- Current album information
- Basic playback controls
- Existing API connectivity
- Mock data fallback for local development

Compile and run available tests.

### Stage 3 — Backend integration

Integrate:

- Real library API
- MPD playback controls
- Artwork cache
- Search
- Track listing
- Favourites
- Playlists
- Now-playing state

### Stage 4 — Complete feature migration

Implement remaining original UI functions, including:

- Settings
- Audio output selection
- Radio
- NAS
- WiFi
- Bluetooth
- AirPlay
- Services
- Storage
- Shutdown/reboot
- Export features

Track actual feature parity.

### Stage 5 — Bootable OS

Integrate the native app into the existing Linux-based appliance architecture.

Create one working Linux target first, preferably i386 or amd64 based on available build/test capability.

Once validated, expand to armhf and arm64 using appropriate OS bases.

### Stage 6 — Optimisation and benchmarking

Optimise memory, CPU, artwork loading, event handling and service startup.

Compare against the Chromium edition on identical physical hardware wherever available.

### Stage 7 — Release engineering

Create and validate all four image build pipelines.

Publish only clearly marked development/prerelease images until physical hardware testing is completed.

## 21. Git commits and pushing

After each meaningful milestone:

- Run applicable tests.
- Review `git status` and the staged diff.
- Confirm the Git remote is PiTunes-Native.
- Commit source code, scripts, documentation and workflows.
- Push the commits to the new public repository using the available Git authentication.

If GitHub authentication is unavailable, do not invent credentials or expose tokens. Explain the exact remaining authentication step.

Never commit large generated `.img`, `.img.xz` files or temporary build directories directly into Git history.

Use GitHub Releases or workflow artefacts for distributable OS images.

For ordinary development, pushing to the new repository is authorised by this instruction.

Do not create stable releases or publish production OTA updates without completing their release validation gates.

## 22. Definition of success

The project is complete when:

- The local PiTunes interface is genuinely native C++/Qt Widgets.
- Chromium is not installed or required by the native appliance.
- CoverFlow closely matches the original interface.
- Core playback, library and settings functions operate correctly.
- The original MPD/ALSA playback architecture is preserved.
- The existing remote web interface remains accessible.
- The native player can run on the supported Linux architectures.
- All four target image builders produce correctly structured bootable images.
- Available automated tests pass.
- Hardware-dependent validation is clearly documented.
- Benchmarks can fairly compare native and Chromium editions.
- The original PiTunes repository remains completely unchanged.
- The PiTunes-Native repository contains the implemented source, documentation, test infrastructure and appropriate workflows.
- All commits are pushed successfully to the new repository.

Do not equate code compilation with complete hardware validation.

## 23. Execute now

Begin immediately.

First inspect the existing local workspace and new GitHub repository.

Then inspect and audit the original PiTunes source.

Safely establish the new PiTunes-Native project, import reusable components, create the persistent project instructions, set up the C++/Qt native application, implement the first functional CoverFlow prototype and run the available tests.

Continue implementing the architecture and build system as far as the available environment permits.

Push completed, tested milestones to:

https://github.com/BASF2HD/PiTunes-Native

At the end, provide a factual report showing:

- Files and modules created
- Features implemented
- Features remaining
- Tests actually executed
- Build results
- Memory/performance results, if genuinely measured
- Git branch and commit SHA
- Whether the push succeeded
- Which target architectures are buildable
- Which target architectures have actually booted on physical hardware
- Any unresolved blockers

Remember: the objective is a working, high-performance PiTunes Native operating system, not merely a proof-of-concept document.
