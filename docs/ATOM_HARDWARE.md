# Intel Atom physical test and recovery

## Evidence from the original USB boot

The 1 GB Intel Atom booted the i386 development image on physical hardware.
Qt opened and the library found albums on the second USB drive. The photos show:

- The kernel bound the `b43` driver on `ssb0:0` and reported `-2` for both
  `b43/ucode15.fw` and `b43-open/ucode15.fw`. The exact PCI ID is still needed.
- The BIOS reported ACPI `AE_ALREADY_EXISTS` in PCI `_OSC`. Boot continued to the
  player. Leave ACPI enabled and correlate graphics, suspend or PCI symptoms
  with a same-boot report before attempting BIOS or kernel changes.
- Qt reported HTTP 502 on `/api/player/play`. That confirms a failed request.
  It cannot distinguish an ALSA open failure from an MPD queue or mount error.
- Startup selected Off on systems with MemTotal at or below 1,100,000 KiB.
  Lightweight now uses that threshold. It still turns and glides the covers.
  The Mac trackpad reports and apparent animation quality require a retest
  with real gestures and artwork on each host.

## Get a report from the Atom

Boot the development USB, wait for the player and allow about two minutes
for the boot report. Open **Settings → Network** to read the detected interface,
radio state and firmware hint. Try a Wi-Fi scan and try your usual music at low
volume. Shut down from **Settings → System** before removing the boot USB.

On the Mac, open the boot USB's FAT partition `PITUNES_EFI`. Copy
`pitunes-hardware.txt`, plus `pitunes-display.log` when display recovery failed.
The files identify the source SHA, CPU/model, boot mode, RAM, USB link speeds,
PCI IDs and bound drivers, ALSA devices and mixer states, and service results.
Kernel, LightDM and MPD logs are classified into fixed counters. The export omits
raw journal lines, hostnames, station names, credentials, network addresses,
device serials and music paths. A Linux administrator can refresh the export
after reproducing a failure with:

```sh
sudo systemctl start pitunes-pc-hardware-report.service
```

The reporter verifies that `/boot/efi` is a separate FAT partition on the disk
that contains the running root filesystem. It never writes to the music drive
or chooses another USB disk. It replaces the report atomically. The automatic
capture runs once after boot and can predate a later playback failure.

## Restore the b43 firmware

The PC image includes Debian's `firmware-iwlwifi`, `firmware-realtek`,
`firmware-atheros`, and `firmware-brcm80211` redistributable packages. The Broadcom
package supplies separate drivers and does not provide the requested b43 image.
Match the collected PCI ID and revision against the supported table before
choosing the package or extracting firmware.
[Linux wireless b43 documentation](https://wireless.docs.kernel.org/en/latest/en/users/drivers/b43.html)
[Debian firmware-b43-installer](https://packages.debian.org/bookworm/firmware-b43-installer)

For a supported chipset and wired access, a Linux administrator can enable
Bookworm's `contrib` component on the existing `deb.debian.org/debian bookworm`
entry, run `sudo apt-get update`, and run
`sudo apt-get install firmware-b43-installer`. The installer fetches and extracts
the required firmware per machine. Reboot to re-probe the adapter. On a PC
without Ethernet, use Debian Live with networking and administrator access to
obtain the matching firmware. In that session identify the **PiTunes boot USB**
by size, label and partitions using `lsblk -o PATH,MODEL,TRAN,SIZE,FSTYPE,LABEL`.
With the PiTunes root partition mounted deliberately, copy the acquired
`b43/*.fw` files into its `lib/firmware/b43/` directory, then unmount and boot it.
Do not put Wi-Fi credentials or vendor firmware into the project repository.

After reboot check interface and rfkill, then scan, connect, verify DHCP and play
a saved station. An adapter that cannot enter AP mode may still work as a
station. Recovery hotspot success on that particular adapter needs testing.

## Verify PC audio

In **Settings → Audio**, choose the detected analogue PCM, such as the one with
an Analog suffix on the HDA controller. Choose **software** volume initially and
apply. Test HDMI or USB DAC as separate PCMs if present. A single analogue PCM
may service both internal speakers and the headphone jack. Observe the exposed
Speaker, Headphone and Auto-Mute mixer states and unplug headphones for the
first speaker test. For hardware-volume control, select a control reported by
the chosen card. Applying a route restarts MPD and can interrupt playback.

For an administrator troubleshooting silence, `aplay -l` lists PCM devices,
`amixer -c <card-number> scontents` lists exposed mixer capabilities, and
`journalctl -b -u mpd.service` distinguishes service startup failures from
device negotiation. The service log and MPD log can contain private file paths.
Use the exported classified report when sharing evidence.

**A speaker test emits audible sound.** Lower levels first, remove headphones,
and stop normal playback before running a short, deliberately chosen test.
For example, after substituting the actual analogue card identifier and device
number, use `speaker-test -D plughw:CARD=Intel,DEV=0 -c 2 -t sine -l 1`.
If it reports that the device is busy, stop the playback service deliberately
before retrying and start it again afterward. No test sound runs automatically.

## Internal SSD installer design

Continue using the persistent USB while the physical audio, wireless, input and
performance fixes are verified. A future installer should boot from USB and
require a deliberate selection of an internal SATA/NVMe disk. Show its model,
capacity, current partitions and firmware boot mode. Exclude the live boot
disk, mounted disks and removable music media. Recheck target identity after
the confirmation prompt and immediately before committing an expanded
partition plan. Confirm destructive partitioning by typing the selected target
identifier. Abort on open holders, partition changes or EFI mismatch.

Validate installation on disposable disks with extra attached data drives.
Cover legacy BIOS and the applicable IA32 or x64 UEFI path, including cold boot
with the installer USB removed. Test cancellation, target hot-unplug, write
failure and power loss. The original USB is the recovery environment, and
recovery must support inspecting and repairing the selected disk without
modifying the others. Release remains gated on those tests and the physical
hardware acceptance checks. There is no internal-disk write path in this build.

## Updated on-device workflow

The regular template now opens in Full animation mode. Settings → System offers
15, 20 and 30 FPS frame limits. The renderer keeps the chosen geometry and
reduces temporary raster sampling density when a frame overruns its budget.
The panel stops repainting at rest. 15/20/30 are limits.

In PC Audio, select the analogue **Output device**. Automatic also resolves
a detected built-in analogue PCM before the MPD service starts. Selecting the
analogue PCM explicitly exposes the reported speaker and headphone settings.
Auto-Mute is the codec's own jack-sense behavior. The test button plays a
half-second low-level tone only after confirmation and while MPD is stopped.
Existing mixer levels are left alone at boot.

When macOS sees the USB under `diskutil list external physical` but Finder does
not mount it, identify the FAT EFI partition by its size (134 MB) and use
`diskutil mount readOnly` with that partition identifier. The larger Linux
partition stays unmounted on macOS. Decline any Initialize prompt. If the
drive disappears from Disk Utility, the partition cannot be read until the
USB connection is re-established. Debian or antiX Live can mount the FAT
partition and retrieve the same report. Network settings now also displays
wireless PCI IDs when a driver has failed to bind.

## Offline Broadcom STA path

Debian Bookworm's `broadcom-sta-dkms` lists BCM4312. Kernel modules compiled
for antiX's 5.10 kernel cannot run against the Debian 12 PiTunes kernel.
Prepare the module before the first PiTunes boot by using a disposable
Debian Bookworm i386 system with network access. Enable Debian `non-free` only
in that preparation system. Install `broadcom-sta-dkms` and the exact
`linux-headers-<release>-686-pae` for the **PiTunes** image, then run
`sudo dkms autoinstall -k <exact-release>`. Check the output with
`modinfo -k <exact-release> -F vermagic wl`. The image's kernel release can be
read from its Linux partition under `lib/modules/`.

For an offline first boot, copy the resulting release-matched module into the
same kernel's `lib/modules/<exact-release>/updates/` on the mounted PiTunes
Linux **USB** partition from antiX/Debian. Run `sudo depmod -b <PiTunes mount>
<exact-release>` to index it. Keep the primary disk untouched. This avoids
installing the Debian package's global module blacklists into PiTunes. When a
local `wl` is available, the i386 service can prefer it specifically on
`14e4:4315` and restore that device's previous binding on failure. Other Wi-Fi
devices keep their drivers. Validate the exact PCI ID on the Atom before use.

The public image does not contain the Broadcom STA binary. Debian distributes
it in `non-free` under a separate licence that conditions redistribution on
the complete, unmodified software, accompanying terms and other obligations.
An owner can prepare the driver locally from Debian. For the b43 alternative,
place the already-extracted official `b43` firmware files in `INSTALL-B43` on
the boot FAT partition. The explicit folder triggers one installation when
the b43 driver is present. Read `b43-install.txt`, remove the folder and
reboot. Choose one driver strategy according to the hardware and available
firmware. Neither driver selection has been physically verified in PiTunes.

[Debian Broadcom STA](https://packages.debian.org/bookworm/broadcom-sta-dkms)
[Broadcom licence distributed by Debian](https://metadata.ftp-master.debian.org/changelogs/non-free/b/broadcom-sta/broadcom-sta_6.30.223.271-23_copyright)
