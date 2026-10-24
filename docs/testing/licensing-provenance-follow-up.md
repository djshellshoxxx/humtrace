# Licensing provenance follow-up

The Windows package workflow stages source-only MSYS2 recipe archives for the
exact package owners and versions mapped to bundled runtime DLLs. It checks
each official package page's displayed version, requires one approved
source-archive link with a matching versioned filename, validates redirects,
and caps package count, page size, archive size and total download size. Shared
source archives are downloaded once. The package includes a manifest with
package-to-archive mappings, source URLs, byte counts and locally computed
SHA-256 digests.

These MSYS2 archives contain build recipes and patch payloads; they do not
contain the upstream source archives referenced by the recipes. Locally
computed hashes do not authenticate a download. This material is supplemental
provenance and does not complete the corresponding-source or relinkability
review for the Windows distribution. The Windows license and QtBase SPDX
inventory also remains an inventory, not a legal conclusion.

The first native attempt, [CI run 40105892447](https://github.com/djshellshoxxx/humtrace/actions/runs/40105892447),
reached the package collector but failed closed when the geo redirect selected
`ftp2.osuosl.org`. The official [MSYS2 mirror list](https://www.msys2.org/dev/mirrors/)
identifies it as a Tier 1 mirror. The next attempt, [CI run 40106555687](https://github.com/djshellshoxxx/humtrace/actions/runs/40106555687),
passed the mirror check but failed on the GCC source recipe exceeding the
64 MiB per-archive cap. The other five native jobs passed. The current change
raises the per-archive cap to 256 MiB and the aggregate cap to 512 MiB; the
focused tests still pass. The corrected native rerun is pending.

Local verification passes `python3 -m py_compile
tools/package_msys2_sources.py tests/test_msys2_sources.py` and all 12 focused
fixture tests. A direct HEAD request to the exact OSUOSL GCC archive URL from
the saved executor is blocked: its network proxy denied the CONNECT tunnel with
HTTP 403. This environment has not downloaded or staged any live source
archive; the corrected native runner must confirm the page and archive path
before the next Windows package can be accepted.

The repository README describes HumTrace as a software artwork and an audio
analysis project. That label does not replace functional, validation, or
artifact-specific licensing acceptance.
