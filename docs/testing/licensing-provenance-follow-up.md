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

Local verification passed `python3 -m py_compile
tools/package_msys2_sources.py tests/test_msys2_sources.py` and all 11 focused
fixture tests. A live download from the official MSYS2 package host could not
be verified from the saved executor: its network proxy denied the CONNECT
tunnel with HTTP 403. The collector therefore has fixture-based evidence here;
a native GitHub runner must confirm the official-page/archive download path
before the next Windows package can be accepted.

The repository README describes HumTrace as a software artwork and an audio
analysis project. That label does not replace functional, validation, or
artifact-specific licensing acceptance.
