# Source metadata patch validation: 0.2.5

The [0.2.4 download audit](release-artifacts-0.2.4.md) found a Windows embedded
source-revision gap despite passing native runtime tests. The patch does not
change detector, tracking or report algorithms.

The candidate was rebuilt locally on Linux x86-64 with GCC 14.2 and Qt 6.8.2.
`ctest --test-dir build-desktop --output-on-failure` passed all 29 suites in
15.23 seconds with JSON Schema validation required. This includes the new
package-source and source-metadata suites, existing core/CLI/streaming cases,
GUI cases and both settings fuzz smoke tests.

The five portable package suites passed 39 tests. The configure-only metadata
suite passed four tests covering twelve actual CMake configurations: supplied
40/64-character revisions without Git discovery, malformed override rejection,
an archive without Git, and real-checkout discovery/fallback. No cases were
skipped. The package-source suite covers bounded reads, CRLF, mismatched/unknown,
absent/oversized metadata and rejection before executable launch.

Native CI passes `HUMTRACE_SOURCE_REVISION=$GITHUB_SHA` in every package build.
Installed Linux and extracted Windows/macOS package smoke tests require the
embedded source file to match that same checkout. The exact-tag CI link and
downloaded asset checks establish the final native evidence; this local record
does not claim native Windows/macOS execution.

The broader stress measurements and open field/human/licensing gates remain in
[0.2.4 QA](qa-0.2.4.md), [desktop QA](desktop-0.2.4.md) and the
[acceptance ledger](../release/acceptance-0.2.4.md).
