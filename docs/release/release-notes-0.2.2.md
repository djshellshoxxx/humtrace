# HumTrace 0.2.2 development preview

Includes the 0.2.1 cross-platform export fixes and adds an explicit bounded-memory
long-file JSON workflow:

```
humtrace recording.wav --stream --json report.json --memory-limit-mib 64 --disk-limit-mib 1024
```

- Shared DSP and deterministic peak association produce the same parsed JSON as
  the capped in-memory analysis on stereo tone, silence, dropout, noise and
  crossing-tone fixtures.
- Frames and track observations spool to a private directory on the destination
  filesystem. Exact medians use bounded disk scans, with no observation tree kept
  in memory; final JSON is assembled incrementally and published atomically.
- Active-track/candidate memory, temporary plus final output bytes and free-space
  reserve are bounded. Hashing, DSP, disk scans and report finalization support
  cancellation and remove private scratch/partial exports on failure.
- Linux acceptance: persistent-tone recordings of one and three hours produced
  14,062/42,187 complete frames, approximately 70/209 MB JSON and 8.71/9.25 MiB
  peak RSS, under a real 96 MiB address-space limit. Original WAV hashes remained
  unchanged. These are implementation measurements, not field-validation claims.

The desktop, CSV export and recording comparison still use capped in-memory
results. Disk-backed GUI presentation/tile eviction remains required work. The
streaming option currently accepts one recording and JSON output. Native preview
packages are standalone applications; no VST3/CLAP adapter is included. Windows
binaries are unsigned; macOS packages are ad-hoc signed and not notarized.

Held-out field-corpus validation, full report provenance/import, packaged
accessibility, clean-machine upgrades and other beta checklist gates remain open.
This preview must not be represented as completing all specifications or as
forensic source identification. See [completion audit](../audit/cloud-completion-audit.md).
