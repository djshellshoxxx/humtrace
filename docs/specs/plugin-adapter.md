# Plugin adapter scope and release gates

**Status:** format and first host choices are selected for implementation.
There is no VST3 or CLAP adapter, plug-in bundle, SDK dependency, host
validation, or supported-host claim in this repository yet. The CLI and Qt
desktop application remain standalone products.

## Release-platform relationship

Both formats are in scope because the requested product workflow is live DAW
capture followed by offline analysis. The first manual host target is REAPER
7.83 on each native package baseline: Linux x86-64, Windows x86-64, macOS
ARM64, and macOS Intel. Cockos lists VST3 and CLAP hosting and these platform
families; that establishes format applicability, not HumTrace compatibility.
Update the host version when making each release and verify every format,
architecture, and platform in the actual host. [REAPER's current download
page](https://www.reaper.fm/download.php) lists version 7.83 on 2026-10-24;
[its format list](https://www.reaper.fm/about.php) includes VST3 and CLAP.

## Format applicability

Build both formats against the pinned VST3 SDK 3.8.1 commit
`3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96` and CLAP 1.2.10 commit
`195b42a004144fab0b3cf95e9c067187d15365b7`. VST3 3.8 uses MIT terms;
earlier SDK releases had different terms. The CLAP API is MIT licensed. Keep
only required API headers and license notices in the build/package; do not
enable VSTGUI, examples, or auto-install. Audit exact SDK source and component
licenses during implementation. Sources: [Steinberg VST 3.8 announcement](https://www.steinberg.net/press/2025/vst-3-8/),
[VST3 SDK](https://github.com/steinbergmedia/vst3sdk),
[CLAP API](https://github.com/free-audio/clap).

| Format | Platform intent | Applicability and gate |
| --- | --- | --- |
| VST3 | Linux x86-64, Windows x86-64, macOS ARM64 and Intel | Selected. Native build, official validator, adapter host harness, REAPER capture/pass-through testing, and packaged artifact checks remain open. |
| CLAP | Linux x86-64, Windows x86-64, macOS ARM64 and Intel | Selected. Native build, official validator, adapter host harness, REAPER capture/pass-through testing, and packaged artifact checks remain open. |

No other host is in the first support scope. A host becomes supported only
after the same evidence passes on that host. The evaluation should use an
existing REAPER license; do not purchase a license or enter a new agreement for
this work. If a suitable installed host is unavailable, report the manual host
gate as pending.

## Adapter boundary

The plugin would be a thin host bridge to a reusable asynchronous analysis job;
DSP, measurement units, limits, report serialization, and qualified result
wording remain in the shared analysis layer. Keep host SDK types out of core
headers and keep a no-SDK standalone build. Do not place Qt widgets, file
dialogs, logging, allocation, locking, file I/O, or analysis calls on the host's
real-time audio callback.

The selected interface and state transitions are in
[plugin-capture-interface.md](plugin-capture-interface.md). It specifies a
non-modifying pass-through effect, bounded capture, worker-thread processing,
explicit dropped-audio provenance, and conservative channel, rate, duration,
and host-block boundaries. Do not claim equivalence with file analysis until
the same captured audio and settings produce validated equivalent output.

## Validation before a support claim

For every claimed format/host/platform combination, validate create/activate,
audio-thread processing, deactivate/destroy, repeated instantiation, sample-
rate and bus changes, variable and zero-sized blocks where permitted, mono,
stereo and any advertised multichannel layout, transport stop/seek, suspend and
resume, offline render, cancellation, state save/restore, and UI open/close.
Force worker overload and capture-limit conditions and verify the job stops
with accurate dropped-frame intervals and a visible incomplete status. Confirm
pass-through audio is unchanged within the format's defined floating-point
semantics. Compare analysis to standalone file input on identical sample
sequences. Run the applicable official format validator, host-specific tests,
package inspection, and signing checks. No such evidence exists today.

## Reuse and prerequisites

Use the [analysis engine](analysis-engine.md),
[long-file workflow](long-file-workflow.md),
[report integrity](report-integrity.md), and
[capture interface](plugin-capture-interface.md). The next implementation
gates are a bounded reusable capture job, accurate capture provenance,
format-specific realtime-safe adapters, validators and host harnesses, then
native package and manual REAPER checks. This scope does not assert that the
currently published standalone release includes a plugin.
