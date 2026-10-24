# Third-party notices and release review status

This file is a draft inventory, not a complete notice bundle or legal
conclusion. It must be reconciled with the exact files in each release
artifact before distribution.

## HumTrace-authored source

The repository has no root `LICENSE` file or other statement selecting a
license for HumTrace-authored code. That is separate from third-party license
obligations: this notice does not choose a project license and does not grant
additional rights to the HumTrace source. No product license or legal
agreement is inferred here.

SHA-256 and FFT functionality used by the dependency-free core are implemented
in-tree, according to the current repository documentation. Packaging scripts,
build-runner tools, and libraries physically included in a platform archive
still require a per-artifact inventory; they cannot be cleared by this source
code statement.

## Qt desktop dependency

The desktop target links Qt 6 Core, Gui, and Widgets. CMake currently requests
Qt 6.4 or later rather than pinning one exact Qt patch release. Linux packages
are built against the runner's Qt; Windows uses `windeployqt6`; macOS uses
`macdeployqt`. The resulting versions, transitive libraries, platform plugins,
and their license options can therefore differ by artifact. The packaging
workflow records installed-version/package metadata and runtime dependency
inventories; these must be reconciled with each actual archive before treating
any inventory as complete.

Qt components are offered under different terms depending on the component
and build, including LGPL, GPL, and commercial terms. Do not apply one blanket
license statement to every bundled Qt library or plugin. For each artifact,
record the Qt version, linked and bundled module/plugin list, corresponding
license option, required license and copyright files, and any source or
relinking materials required by the selected terms. Qt's official
[LGPL text and guidance](https://doc.qt.io/qt-6/lgpl.html) is a starting point;
the exact component and distribution path must be reviewed against its actual
license files. Dynamic linking alone does not close the review. The Windows
workflow is configured to map bundled runtime and nested Qt plugin DLLs to MSYS2 packages,
record source/staged hashes, copy installed package license files, and save the
installed package list and installed QtBase SPDX metadata. Its coverage is the
staged DLL set; it is not a complete per-file license inventory of every executable. Windows
also stages exact-version MSYS2 source-only package archives for bundled runtime
DLL owners. These contain build recipes and patch payloads, not the upstream
source archives referenced by those recipes; their SHA-256 values are computed
locally over downloads and are not upstream-authenticated digests. The recipe
set is limited to runtime owners recorded in `DEPENDENCY_LICENSES.json`.
Windows and macOS workflows stage a SHA256-verified upstream QtBase source archive, its
`LICENSES` tree, and a scope manifest matched to the installed Qt version.
macOS also records Homebrew metadata and the installed QtBase formula, including
its patch references. The current packaging change adds a candidate mapping for
each final Mach-O image using exact filenames found in installed Homebrew
Cellar kegs. The package records unique, ambiguous, and unmapped results,
formula license/source fields, hashes of matching keg files, and installed
formula recipes when available. This is an inventory aid, not proof that a
filename match is the same binary that was copied or a license determination.
The native CI run must validate this new mapping before it is accepted.
The per-image dependency manifest records minimum OS, load commands and final
hashes after signing. The QtBase archive is an upstream baseline; it does not
establish correspondence to distribution-patched binaries or provide source
for other Qt modules and dependencies. Linux declares system shared-library
dependencies and installs this notice. Distribution patches, other Qt
modules/dependencies, applicable license options, corresponding-source and
relinkability obligations still need artifact-specific review. These
inventories do not establish complete corresponding source or legal compliance.
The exact-tag CI result and archive manifests establish build and package-test
evidence. This draft does not certify an archive's licensing status.

A v0.2.3 Windows archive was inspected and contained the two executables and Qt
plugin folders but no root Qt DLLs. Its smoke test ran with runner `PATH`
entries that supplied missing runtime dependencies, so it did not establish
that the archive was self-contained. The Windows dependency closure and an
isolated package smoke test are release blockers until a corrected artifact is
verified. See the [beta checklist](beta-checklist.md).

## Deferred plug-in SDKs

No VST3 or CLAP SDK/API is currently built, linked, or redistributed by this
project. If a plugin is later approved, record and pin the exact VST3 SDK
release or CLAP revision, audit all included helper code and transitive
components, and add their required notices. The [plugin adapter spec](../specs/plugin-adapter.md)
records upstream license references and current deferred status.

## Artifact-specific completion record

Before release, attach an inventory for each OS/architecture package that
lists every redistributed executable, shared library, Qt plugin, license file,
copyright notice, source/relinking offer or materials where applicable, and
the source revision used to build it. Verify the archive contents on an
isolated system without CI runner Qt libraries on `PATH`. Update this notice
from that evidence; a successful compile or source-level dependency list is
not a substitute.
