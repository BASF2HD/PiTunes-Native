# Benchmark evidence

No native target hardware measurements have been collected. No RAM, frame-rate,
boot-speed or audio-quality improvement is claimed yet. PERFORMANCE.md defines
the comparison protocol; benchmark-linux.py is a sampler, not fabricated results.

Mac development measurements, if added, must identify the exact code commit,
toolchain, demo/mock state, display backend and limitations. They cannot be used
as measurements of the Atom or Raspberry Pi appliance.

`macos-demo-process.json`: one actual Release/offscreen Mac demo run, 9.96 s wall,
54.55 MiB process maximum RSS, no API/audio/art downloads, uncontrolled filesystem
caches. Source f538876. This is development-host evidence only.
