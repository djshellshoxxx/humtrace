#!/usr/bin/env bash
# Usage: scripts/smoke_package.sh <package.tar.gz>
# Extracts the artifact into a fresh dir, analyzes a generated WAV with the CLI,
# checks the JSON report, and launches the GUI offscreen (must stay alive 3 s).
set -euo pipefail
pkg=$(realpath "${1:?package tar.gz}")
work=$(mktemp -d); trap 'rm -rf "$work"' EXIT
tar -xzf "$pkg" -C "$work"
root=$(echo "$work"/humtrace-*)
python3 -I - "$work/t.wav" <<'PY'
import math, struct, sys, wave
with wave.open(sys.argv[1], "wb") as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(8192)
    w.writeframes(b"".join(struct.pack("<h", round(12000*math.sin(2*math.pi*60*i/8192))) for i in range(8192*3)))
PY
"$root/bin/humtrace" "$work/t.wav" --json "$work/r.json" --no-progress > "$work/out.txt"
python3 -I - "$work/r.json" "$work/t.wav" <<'PY'
import hashlib, json, sys
r = json.load(open(sys.argv[1]))
assert r["input"]["sha256"] == hashlib.sha256(open(sys.argv[2], "rb").read()).hexdigest()
p = r["channels"][0]["frames"][0]["strongest_peak"]
assert abs(p["frequency_hz"] - 60) < 0.5, p
PY
set +e
QT_QPA_PLATFORM=offscreen timeout 3 "$root/bin/humtrace-gui" "$work/t.wav" >/dev/null 2>&1
rc=$?
set -e
[ "$rc" = 124 ] || { echo "GUI did not stay running (rc=$rc)"; exit 1; }
echo "package smoke test passed"
