# Performance comparison protocol

Engineering targets: 200–350 MB idle / <500 MB playback on the 1 GB Atom where
achievable; stable operation within 512 MB on Zero 2 W; responsive 20–30 FPS cover
transitions; stopped idle rendering. These are targets, not measurements.

Use identical hardware, USB/SD/SSD, firmware settings, resolution, audio route,
music library and optional services for original dd34c46 and native candidate.
Record exact image/source/hash/package manifest. Separate physical hardware from
VM/QEMU and Mac development measurements. Record cold power-on vs reboot, first
library scan vs warm index, and cold vs cached artwork/search explicitly.

For each candidate measure at least three runs of: power-on to first usable input,
idle and playback RAM/RSS/CPU, cover navigation frame times/input latency, artwork
decode/load delay, search latency, scan interference, disk I/O/page faults/swap,
audio underruns, and prolonged playback while browsing/scanning/exporting. Use
camera timestamps for firmware-to-input latency and actual listening/DAC tests;
systemd-analyze alone excludes firmware and usable-input readiness.

Linux counters:

```sh
python3 tools/benchmark-linux.py --kind physical \
  --hardware 'Atom model, 1 GB, storage, display, DAC' --scenario idle \
  --pid NATIVE_PID --pid API_PID --pid MPD_PID --seconds 30 \
  --output .cache/benchmarks/native-idle.json
```

Counter sampler records MemAvailable (not the old incorrect free column), process
RSS/high-water/swap, cumulative CPU seconds and raw I/O/VM counters. Process CPU
utilization must be calculated from successive samples, not absolute CPU seconds.
Raw diskstats include partitions; do not sum them indiscriminately. Frame times,
audio underruns and power-on timing are explicitly not instrumented by this script.
Add external tracing and logs for those; do not turn an absent counter into zero.

Review diagnostics for private device/network/path data before copying selected
results into `docs/benchmarks/`. Source-control only reviewed evidence, not raw
private logs. A GUI screenshot proves rendering, not physical performance.

## Chromium versus native on the 1 GiB Atom and 512 MiB Zero 2 W

Use the same hardware, display resolution, output device, USB music library and
library scan state on both builds. Record build ID, OS/architecture, audio format,
and whether reduced motion is enabled. Keep first boot and initial scan results
separate from steady state. Use three cold boots per build at minimum, and capture
the same run of CoverFlow gestures on both builds.

| Metric | Capture |
|---|---|
| Total RAM | After the scanner settles, record `MemTotal - MemAvailable` from `/proc/meminfo`, plus swap usage. Capture idle and playback. If reporting per-process memory, use PSS where available. |
| Boot time | Measure power-on to a responsive first cover using video. Save `systemd-analyze time` and the boot journal for component timing. Firmware and time until a clickable cover are separate measurements. |
| CoverFlow responsiveness | At the same window size and selected library position, record single-step and repeated scroll gestures. Compare the time from input to the first visible cover movement and time until it settles. Report median and worst observed response, plus any dropped/stalled transitions. |
| Playback CPU | Use the same file and output, no scanning and no browsing. Sample system CPU and the player, MPD and API for 60 seconds after playback stabilizes. Normalize process CPU consistently across both builds. |
| Idle CPU | Leave the same cover visible for 60 seconds with playback stopped. Use the same CPU sampling procedure. |

For system CPU snapshots, `/proc/stat` provides cumulative counters. The
difference over the sample window provides an average; `top -b -d 1 -n 61`
can supply per-process samples with the first reading excluded. A 1 GiB native
image smoke test can establish boot and playback under a memory cap. Real Atom
and Zero 2 W RAM, latency and CPU results require measurements on those devices.

## Built-in System Performance Monitor

**Settings → System → Performance Monitor** selects Off, Compact overlay or
Detailed monitor. **F12** toggles the compact overlay on or turns the monitor
off. Off is the startup default. The opaque text window updates once per second
from Linux `/proc` and `/sys`. The monitor uses no background service or shell
commands. A reading of n/a means the data was unavailable on this system.

The monitor calculates RAM used as MemTotal minus MemAvailable. Process CPU
uses each process's user and system ticks divided by the elapsed all-core system
ticks. Disk throughput uses whole-device sector deltas (512-byte sectors), so
disk activity includes the boot USB and other active physical drives. The
hardware report lists USB link speeds. ZRAM shows original and resident memory,
separately from total swap. The first cover metric is time from construction of
CoverFlow to its first paint with centred decoded artwork, including API time.
This is distinct from complete cold boot and excludes firmware time.

Animation FPS counts completed CoverFlow paint events per second of actual
transition time within the sample. This is widget rendering, independent of
display refresh. Average and peak report measured QPainter work in animation
frames. Idle reports zero FPS. Selecting Off stops the sampler and paint
timing. The overlay has a separate backing surface from CoverFlow.

**Record benchmark CSV…** captures up to 900 one-second samples and writes the
file once when stopped, switched Off, or automatically filled. Put the CSV on
a separate writable drive when benchmarking the boot USB. It records elapsed
animation time, frame count, CPU, RAM, disk, swap activity and first cover delay.
Keep the monitor Off and use the external sampler for monitor-overhead comparisons.
Compare matching 60-second idle, playback and repeated CoverFlow scenarios
with the monitor Off, Compact and Detailed, recording total CPU and the Qt
process RSS independently. Repeat in reversed order on each physical machine.
The requested less-than-2 MiB RSS and less-than-1 percentage point total CPU
targets still require physical measurements.

## Development rendering measurements

Local Qt 5 Release on Apple M5 Pro, Qt offscreen raster backend, 1000×610
viewport at device pixel ratio 1, 16 synthetic PNG covers served over loopback.
Each animation run used 12 adjacent-cover transitions with 455 ms between
selections. Process CPU is a fraction of one CPU core over the whole sequence.

| Mode | Animation paints | Active FPS | Mean paint | Peak paint | Process CPU |
|---|---:|---:|---:|---:|---:|
| Full | 302 | 59.05 | 6.05 ms | 12.16 ms | 35.07% |
| Lightweight | 144 | 27.55 | 4.31 ms | 5.55 ms | 13.64% |

Separate 9-second idle samples using the same setup measured process CPU at
0.15% Off, 0.37% Compact and 0.31% Detailed. Compared with the RSS immediately
before enabling the monitor in each run, Compact grew by 272 KiB and Detailed
by 1,376 KiB. The stage painted once on opening each panel and had zero paints
while idle with the monitor Off. Linux counters and Xorg performance are absent
from this Mac measurement. Confirm the monitor and CoverFlow on both physical
targets before using these values to set a release threshold.

A repeat in reversed order measured 5.80 ms mean / 13.12 ms peak in Full and
4.33 ms mean / 5.77 ms peak in Lightweight, with 33.58% and 13.74% one-core
process CPU respectively. Idle process CPU was 0.24% Off, 0.35% Compact and
0.37% Detailed, with measured RSS growth of 272 KiB Compact and 1,344 KiB
Detailed. Each active panel again caused one initial stage paint.
