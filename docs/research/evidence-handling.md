# Evidence handling, hashes, and report boundaries

**Reviewed:** 2026-10-08. This note translates evidence-preservation guidance into software requirements. HumTrace is not a validated forensic acquisition or chain-of-custody system.

## What the product can support

HumTrace can compute a cryptographic digest of the input bytes it analyzed, record application/build and method/settings metadata, save an analysis report, and provide an append-only analyst note log with timestamps. It can show whether a file's bytes match a prior digest. It cannot prove who acquired a file, where it came from, whether the acquisition was complete, whether an analyst controlled the workstation, or whether a report has not been altered unless the operational process supplies trusted signatures, custody controls, and independent verification.

NIST's evidence-preservation report discusses preservation of digital objects and the need to manage integrity and custody as part of a process. A file hash is useful for detecting byte changes; it does not, by itself, establish provenance or custody. HumTrace must distinguish the narrow integrity check from those broader claims. [1][2]

## Product requirements

- Hash the exact source bytes, not normalized/decoded samples. Use SHA-256 through a maintained platform or vetted library API. Record algorithm, lowercase hex digest, byte length, and whether hashing completed.
- Hash the source while streaming if possible; otherwise make a separate pass. Record that the hash covers the entire file and verify file identity/size before and after analysis to detect obvious concurrent modification. If source metadata changes during analysis, stop or mark the report invalid/incomplete.
- Never overwrite or transform the input. Write reports to a new destination using a temporary file followed by atomic replace where supported. Preserve an existing report unless the user explicitly confirms replacement.
- Report creation time in UTC as a software clock observation, not trusted acquisition time. Preserve file-provided timestamps only as unverified metadata.
- Include HumTrace version, source revision/build identifier if available, OS/architecture, decoder name/version, DSP method version, all analysis settings, warnings, channel metadata, and supported/unsupported preprocessing.
- Provide a privacy preview: absolute path and user/account information can be sensitive. Default export should support a basename-only or redacted path while always retaining content digest and byte length.
- Analyst notes are user-authored, timestamped annotations. Edits must be explicit and visible in an audit log; HumTrace must not present local notes as independently authenticated.
- Optional report signing is a future feature. It requires a documented key-management/signature workflow and verifier; never imply a plain hash is a digital signature.

## Report status states

`complete`, `cancelled`, `failed`, or `partial`. Every state includes completed stages, warnings, hash status, and whether a report is complete enough to reproduce. Canceled/failed reports must not look like complete results. Schema versioning and a JSON Schema are required for each released report version.

## Sources

1. NISTIR 8387, [Digital Evidence Preservation: Considerations for Evidence Handlers](https://doi.org/10.6028/NIST.IR.8387), 2022.
2. NIST, [Evidence Management](https://www.nist.gov/forensic-science/interdisciplinary-topics/evidence-management), overview of preservation and custody tracking.
3. NIST OSAC, [Standards Library](https://www.nist.gov/standard/1516), includes guidance on confidence/error mitigation in digital and multimedia forensic results. Consult current standards and qualified practitioners before making forensic-use claims.
4. HumTrace [report schema](../modules/report-schema-v1.md) and [limitations](../user-guide/limitations.md).
