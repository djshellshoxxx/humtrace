# Downloaded release artifact audit: 0.2.4

The published [v0.2.4 preview](https://github.com/djshellshoxxx/humtrace/releases/tag/v0.2.4)
was built from `9222189f9572076c5bc7582caae64876679e7a13` by
[exact-tag CI](https://github.com/djshellshoxxx/humtrace/actions/runs/40046403106).
All native build/test/package jobs and the release job passed.

The coordinator then downloaded all eight release assets and checked their byte
lengths and SHA-256 digests against GitHub's asset metadata. All matched. The seven
entries in `SHA256SUMS.txt` also matched the actual downloaded bytes. The source
tarball matched all 644 blobs from the declared Git commit.

The Windows archive contained 42 inventoried DLLs and 77 hashed notice/SPDX
records; every staged binary and notice matched its manifest. The macOS ARM64
archive contained 29 final signed Mach-O images and the Intel archive contained
25; their byte hashes matched their embedded manifests. Audited maximum minimum
OS versions were 26.0 ARM64 and 15.0 Intel. Both Linux binaries were x86-64 ELF,
and both the Linux tarball and Debian package embedded the declared source SHA.

**Acceptance failure found after download:** the Windows ZIP embedded
`unknown (source archive without Git metadata)` in `SOURCE_COMMIT.txt`. CMake's
bare Git invocation was unavailable in that MSYS2 build environment. The separate
release-level source file, tag, source archive and CI trace identify the correct
commit, and the Windows binaries passed native execution tests; this is an
embedded metadata gap. It is not treated as a passing source-file check.

Version 0.2.5 supplies a validated revision explicitly to CMake and checks every
installed/extracted package's source file against the CI checkout before launching
its executables. The 0.2.4 tag and published assets remain unchanged.
