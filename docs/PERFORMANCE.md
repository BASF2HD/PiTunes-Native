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
