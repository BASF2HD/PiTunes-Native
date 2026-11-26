#!/usr/bin/env python3
"""Sample Linux process/system counters; results are never invented or auto-published."""
import argparse
import json
import os
import platform
import subprocess
import time
from pathlib import Path


def counters(path):
    result = {}
    for line in path.read_text().splitlines():
        key, _, value = line.partition(":")
        parts = value.strip().split()
        if parts and parts[0].isdigit():
            result[key] = int(parts[0])
    return result


def process(pid):
    root = Path("/proc") / str(pid)
    try:
        raw = (root / "stat").read_text()
        # comm can contain spaces; numeric fields follow the final close parenthesis.
        fields = raw[raw.rfind(")") + 2:].split()
        return {"pid": pid, "statusKiB": {k: v for k, v in counters(root / "status").items()
                if k in ("VmRSS", "VmHWM", "VmSwap")},
                "cpuSeconds": (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK"),
                "io": counters(root / "io")}
    except (OSError, ValueError, IndexError):
        return {"pid": pid, "unavailable": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kind", choices=("physical", "virtual"), required=True)
    parser.add_argument("--hardware", required=True, help="Non-sensitive model / RAM / storage description")
    parser.add_argument("--scenario", required=True, help="idle, playback, navigation, scan, etc.")
    parser.add_argument("--pid", action="append", type=int, required=True)
    parser.add_argument("--seconds", type=int, default=30)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if platform.system() != "Linux": parser.error("Linux /proc counters are required")
    if not 1 <= args.seconds <= 3600: parser.error("seconds must be 1..3600")
    if args.output.exists(): parser.error("Refusing an existing result file")
    try:
        commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    except subprocess.CalledProcessError:
        commit = "unknown"
    result = {"measurementKind": args.kind, "hardware": args.hardware, "scenario": args.scenario,
              "sourceCommit": commit, "architecture": platform.machine(), "kernel": platform.release(),
              "clock": "monotonic", "audioUnderruns": "not instrumented", "frameTimes": "not instrumented", "samples": []}
    start = time.monotonic()
    for i in range(args.seconds + 1):
        mem = counters(Path("/proc/meminfo"))
        result["samples"].append({"seconds": time.monotonic() - start,
            "memoryKiB": {k: mem[k] for k in ("MemTotal", "MemAvailable", "SwapTotal", "SwapFree") if k in mem},
            "processes": [process(pid) for pid in args.pid],
            "vmstat": Path("/proc/vmstat").read_text(), "diskstats": Path("/proc/diskstats").read_text()})
        if i < args.seconds: time.sleep(max(0, start + i + 1 - time.monotonic()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__": main()
