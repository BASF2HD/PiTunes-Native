#!/usr/bin/env python3
"""Check staged text for high-confidence private material without echoing values."""
import re
import subprocess
import sys

paths = subprocess.check_output(["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR", "-z"]).split(b"\0")
patterns = [
    rb"-----BEGIN (?:RSA |EC |OPENSSH |DSA |ENCRYPTED )?PRIVATE KEY-----",
    rb"\bgh[pousr]_[A-Za-z0-9]{30,}\b",
    rb"\bgithub_pat_[A-Za-z0-9_]{30,}\b",
    rb"\bAKIA[A-Z0-9]{16}\b",
    rb"\bsk-(?:proj-)?[A-Za-z0-9_-]{40,}\b",
]
failed = []
for raw in paths:
    if not raw:
        continue
    path = raw.decode()
    data = subprocess.check_output(["git", "show", ":" + path])
    if any(re.search(pattern, data) for pattern in patterns):
        failed.append(path)
    if any(part in path.split("/") for part in (".ssh", ".aws", ".venv", "__pycache__")):
        failed.append(path)
    if path.endswith((".img", ".img.xz", ".credentials", ".p12")):
        failed.append(path)
if failed:
    print("Private/generated material requires review in: " + ", ".join(sorted(set(failed))), file=sys.stderr)
    sys.exit(1)
print(f"Staged-source scan passed ({sum(bool(p) for p in paths)} files); manual template review is still required.")
