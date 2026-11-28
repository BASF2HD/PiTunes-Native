# Local graphics and browser runtime audit

Scope covers the four native image profiles, installed root filesystems, the X11
kiosk entry points, the native binary and the retained LAN web interface.

## Native boot and rendering

| Stage | Source and behaviour |
|---|---|
| PC firmware | `tools/build-pc-image.sh` creates the Debian root and boot partitions. `image/pc/boot/configure.sh` installs BIOS GRUB and architecture-matched removable UEFI with UUID-based root selection. The LightDM drop-in starts `image/pc/boot/start-display.sh` for bounded Xorg driver recovery. |
| Pi firmware | `tools/build-native-image.sh` verifies the Raspberry Pi OS Lite Bookworm base for ARM. `tools/build-flashable-image.sh` retains its firmware and boots the installed systemd services. `tools/chroot-install.sh` configures the login account and native kiosk. |
| Source staging | `tools/prepare-native-source.py` stages Git-visible source and applies the non-kiosk PC adaptations. Both inner builders and `install.sh` require a native staging marker. The official workflow calls `tools/build-native-image.sh`. |
| Graphical login | `scripts/setup-native-kiosk.sh` enables LightDM, sets `graphical.target`, and selects `pitunes-native.desktop` for both autologin and manual session selection. The PC kiosk account and Pi login account run the session without root UI privileges. |
| Local launch | `scripts/pitunes-native-session.sh` keeps X11 awake, starts Openbox, and starts `systemd/pitunes-native.service` in the user manager. Its only player command is `/usr/bin/pitunes-native --fullscreen` with the XCB Qt platform. Restart limits belong to this user service. |
| Rendering | `CMakeLists.txt` builds C++17 with Qt 5 Widgets, Network, Concurrent and Svg. `native/app/main.cpp` creates a QApplication and QWidget-based MainWindow. `native/coverflow/CoverFlowWidget.cpp` paints projected covers and reflections with QPainter. `native/artwork/ArtworkCache.cpp` decodes bounded images and SVG. |
| Independent services | `systemd/pitunes-api.service` runs the Python API with MPD as a service dependency. nginx serves the LAN client. Mounts, scanner, first boot, Samba, wireless services and update policy do not start a player. |

The Qt executable's default endpoint is the Python JSON/media API at
`127.0.0.1:8080`. `native/networking/ApiClient.cpp` limits requests to
`/api/` paths on the configured server. The executable embeds existing PNG
branding through `native/assets.qrc`. The backend also shares a station SVG
fallback. These assets are images loaded by Qt, while HTML, JavaScript and CSS
are served by `nginx/pitunes.conf` from `/opt/pitunes/frontend` for browsers.
The local session does not use nginx to display a page.

## Findings and changes

- The retired `pitunes-display.service` was disabled but still installed.
  `pitunes-samba-late.service` requested it through `Wants=`. An enabled timer
  could load the old unit even after disablement. Its former compatibility
  script invoked the native session, creating a possible second launch path.
  The shipped unit is now masked, the retained source unit refuses manual
  start, both legacy launcher scripts fail closed and Samba and scan/hotspot
  ordering no longer refers to the retired unit.
- The previous package gate covered four exact names and ran during kiosk
  setup. `tools/audit-native-rootfs.py` checks the final dpkg installed
  status, broad browser and WebEngine/WebView/WebKit/CEF package families,
  native ELF dependency evidence, LightDM wiring, the mask and the retained web
  files. Builders fail on a violation and export the final package manifest,
  linked libraries and rootfs audit beside the image and checksum.
- Browser-only PC launcher/installer patches and the unused historical
  browser rendering optimisation are under `docs/reference/pc-browser-patches/`.
  Historical release workflows remain under `docs/reference/workflows/`.
  The disabled legacy deployment helper no longer carries an unreachable
  browser-service restart. The archived patches are outside the image staging
  inputs. The remote site's source and existing PC adaptations are retained.
- Qt Network, SVG, XCB, Xorg, Openbox, GTK greeter, and optional system graphics
  libraries do not imply a browser engine. The offscreen package smoke uses
  the installed binary. The PC image smoke additionally observes the actual
  process table in a disposable copy, with the original image checksum checked.

## Workflows and evidence

`native-ci.yml` builds and tests the native DEB in all four target distro
containers. `native-images.yml` builds the disk in a root Linux runner and
uploads development bundles. `native-image-audit.yml` mounts the second
partition of existing artifacts read-only and captures the installed-package
database and boot wiring. `native-smoke.yml` runs the original BIOS/UEFI
and playback checks. In the audited version, the process probe samples after
the Samba timer and checks a single unprivileged fullscreen player, one Openbox
instance, and zero recognized browser processes. The LAN guest also checks
HTTP delivery of HTML, JavaScript and CSS. The probe is added only to a test
copy.

Evidence links and exact results are recorded in [PROGRESS.md](PROGRESS.md).
The PC VM smoke is separate from the Atom and Zero 2 W physical performance
comparison in [PERFORMANCE.md](PERFORMANCE.md).

## Downloaded image inspection

The [read-only artifact inspection](https://github.com/BASF2HD/PiTunes-Native/actions/runs/46091020660)
mounted the checksum-verified i386, armhf and arm64 root filesystems from
`38161d0`. All three reports show an empty forbidden-package list, the legacy
launcher masked, no unexpected legacy references, captured ELF dependencies,
and the native executable as the user-service command.

| Architecture | Installed packages | Rootfs evidence |
|---|---:|---|
| i386 | 597 | [Read-only report](https://github.com/BASF2HD/PiTunes-Native/actions/runs/46091020660/artifacts/15241341004) |
| armhf | 896 | [Read-only report](https://github.com/BASF2HD/PiTunes-Native/actions/runs/46091020660/artifacts/15241066547) |
| arm64 | 882 | [Read-only report](https://github.com/BASF2HD/PiTunes-Native/actions/runs/46091020660/artifacts/15241466298) |

The earlier amd64 artifact URL returned HTTP 404 during retrieval. amd64
package testing remains available through Native CI.
