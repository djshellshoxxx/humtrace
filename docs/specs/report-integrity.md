# Analysis report, provenance, and integrity specification

## Report goals

The report makes an analysis reproducible and its limits visible. It is not a certificate, evidence acquisition record, or chain-of-custody system. Follow the specific boundaries in [evidence-handling research](../research/evidence-handling.md).

## Required top-level sections

- `schema_version`, application/version, build revision, OS/architecture, report status, UTC report-generation time, and method versions.
- `input`: optional privacy-redacted path, exact byte length, SHA-256 algorithm and digest, hash completion status, container/codec, sample rate, channels/layout, bit depth/valid bits, duration, and decoder version.
- `settings`: full serialized settings, units, window, frame/hop, PSD configuration, detrend/DC rules, detector thresholds, tracker parameters, and all requested transformations.
- `results`: channel/frame/track/event measurements with units, uncertainty/status flags, evidence components, missing data, and warnings.
- `notes`: analyst-authored notes and timestamps with explicit unauthenticated/local provenance.
- `integrity`: report schema identifier; optional report-file digest is computed after finalization and stored separately to avoid self-reference.

## Serialization rules

Schema uses strict types and no NaN/Infinity. Unknown future fields are ignored only where schema compatibility policy says so; unknown major versions fail clearly. Large reports are streamed to a temp file, flushed, validated, then atomically renamed. Failure/cancellation leaves no file that appears complete. Include a schema migration utility and golden fixtures for each released version.

## Hashing and modification checks

Hash exact file bytes with SHA-256 while reading or in a separate pass. Record bytes processed and full-file completion. If file identity/size changes during analysis, mark result `invalidated` or stop. Hash detects equality to a known byte sequence; it does not establish source, acquisition time, authorship, or custody. Report file-provided timestamps as unverified metadata.

## Export privacy and analyst workflow

Before writing, preview output path, schema version, warnings, input digest, and whether absolute paths or notes will be exported. Default to basename-only path. Preserve the source and existing report. Overwrite requires an explicit UI confirmation. A future signing feature requires a verifier and key-management design; SHA-256 alone is not signing.

## Tests

Validate against published JSON Schema; parse round trips; test redaction, unicode/control chars, large streaming report, hash changes, changed input during read, incomplete/cancelled output cleanup, atomic replacement, unknown schema major, and exact digest known-answer vectors. Keep sample reports synthetic or consented and redact sensitive file paths.
