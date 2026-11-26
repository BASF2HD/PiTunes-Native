# Native application architecture

`native/app/main.cpp` creates QApplication and an unprivileged Widgets window.
Qt's QWidget raster backing store paints CoverFlow; no QOpenGLWidget, WebEngine,
WebView or browser subprocess is used. Qt GUI itself may link optional platform GL
dependencies; no functioning GPU is required by the app's painting code.

- `models/Domain`: normalizes numeric/string IDs, nested/flat MPD state, mixed
  booleans, volume -1 and track artist. No playback engine or library DB.
- `networking/ApiClient`: async same-origin API, manual redirects, capped responses,
  per-request timeout, JSON/body errors. No automatic mutation retry or credentials
  embedded in URLs.
- `models/LibraryModel`: QAbstractListModel, metadata pages of 200, independent
  total and stale generation guard. Metadata can exceed 5,000 albums; artwork is
  not loaded per row. Failed API never silently selects demo playback.
- `artwork/ArtworkCache`: byte-costed QCache LRU, 6/12 MiB profiles, hard 4..32 MiB
  configuration bounds, two network/decode jobs and eleven desired prefetch keys.
  QThreadPool scales/decodes and generates reflections off GUI thread. Compressed
  response <=4 MiB, reject source art above four megapixels, normal output 320 px (256 px on low-memory profiles).
  Plugin transient buffers are additional to cached-byte budget.
- `coverflow/CoverFlowWidget`: <=9 covers, QTransform projection, cached reflections,
  time-based .42 s cubic easing at <=30 Hz, timer stops at rest. Unchanged player
  polls do not invalidate flow. Fine-grained dirty rectangles remain an optimization.
- `ui/MainWindow`: browse, virtual lists, transport intents, nonoverlapping 1.5 s
  polls, stale guards across commands, full URI queues. Continuation stays backend-owned.
- `ui/SettingsDialog`: async forms, cleared secret fields, capability-populated
  ALSA/HAT selection and returned errors/reboot warning. No shell commands. Hardware
  forms remain provisional until validated against real appliance responses.
- `ui/TouchKeyboard`: focus-aware keyboard within active dialogs. Language/IME,
  complete focus testing and small-screen layout remain parity work.

Remote nginx/frontend paths and API contracts remain. Add compatible extensions,
never a second queue or music store. Demo has no audio; mock validates fake state.

Tests cover adapter types/errors/timeouts, no mutation retry, URL/size bounds, stale
generation, 5,001 albums, corruption/LRU limits, bounded jobs/destruction, cover
input and idle painting. Real MPD/audio/touch/boot/resources are separate gates.
