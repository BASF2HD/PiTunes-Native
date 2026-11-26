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
