# Third-Party Notices

The Repeat, Repeat-1, and Shuffle SVG icons in `frontend/index.html` use the
official [Lucide](https://github.com/lucide-icons/lucide/tree/main/icons) paths.

## Lucide ISC License

Copyright (c) 2026 Lucide Icons and Contributors

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

## Native Qt dependencies

The native application dynamically links Qt 5 Core, GUI, Widgets, Network,
Concurrent, SVG and Test (tests only), under LGPLv3/GPL/commercial licenses as
applicable. PiTunes' MIT license does not relicense Qt. See
https://www.qt.io/licensing/open-source-lgpl-obligations and
https://code.qt.io/cgit/qt/qtbase.git/tree/LICENSE.LGPL3?h=5.15.
Distributions must retain Qt notices, corresponding-source availability and the
ability to replace/relink LGPL libraries. No static Qt build is shipped here.

The remote interface retains original Three.js (MIT) and GSAP vendor headers.
Review their exact bundled-version licensing before publishing images; retaining
headers alone is not a completed third-party distribution audit.

The optional Windows standalone target also uses:

- miniaudio 0.11.23 under the MIT No Attribution option provided upstream.
  Source and licence: https://github.com/mackron/miniaudio/tree/0.11.23
- TagLib 1.13.1 under its MPL 1.1 option. The source is fetched unmodified
  from https://github.com/taglib/taglib/tree/v1.13.1 and the portable build
  includes both upstream licence texts.
- Qt 5.15.2 and the matching MinGW 8.1 runtime DLLs from the Qt distribution
  archive. Qt modules are linked dynamically and can be replaced with
  compatible versions. Sources: https://download.qt.io/archive/qt/5.15/5.15.2/single/
