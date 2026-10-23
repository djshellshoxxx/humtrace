# Deferred plugin adapters

**Status:** specified for evaluation only. No VST3 or CLAP adapter, plug-in
bundle, SDK dependency, host validation, or supported-host claim is present in
this repository. The CLI and Qt desktop application are standalone products.

## Release-platform relationship

Any future plugin release is intended to target the same Windows, macOS, and
Linux platform families as the standalone release. That is a release goal, not
evidence that a particular host, operating-system version, architecture, or
plugin format is supported. The standalone and plugin gates remain separate:
each format/host/platform combination needs a native build, format validator,
load test, capture test, and packaged artifact check before it can be named as
supported. See [the proposed capture interface](plugin-capture-interface.md)
for the format-neutral contract and the unresolved host/workflow decision.
Where the standalone release verifies Linux x86-64, Windows x86-64 (UCRT64
build), and both Apple silicon and Intel macOS, a plugin format inherits
those targets only after its own architecture-specific artifacts and host
checks pass.

## Format applicability

VST3 and CLAP are candidate delivery formats, not selected dependencies. The
VST3 3.8 SDK is published under MIT terms by Steinberg; earlier SDK releases
had different licensing, so any adoption must pin and audit an exact release.
The CLAP API is published by free-audio under MIT terms. SDK/API license terms
do not establish host availability, user demand, or compatibility. Review the
exact pinned source, notices, build integration, and any helper libraries
before adoption. Sources: [Steinberg VST 3.8 announcement](https://www.steinberg.net/press/2025/vst-3-8/),
[VST3 SDK](https://github.com/steinbergmedia/vst3sdk),
[CLAP API](https://github.com/free-audio/clap).

| Candidate | Platform intent | Applicability and gate |
| --- | --- | --- |
| VST3 | Windows, macOS, Linux, subject to per-host/platform verification | A plausible route for DAW-hosted capture, but no target host list or installed host validation is recorded. Pin an SDK release and use its matching validator and lifecycle guidance. |
| CLAP | Windows, macOS, Linux, subject to per-host/platform verification | A plausible alternative where target hosts implement CLAP. No target host list or host validation is recorded. Pin an API revision and test the selected host/extension subset. |

Do not implement both formats merely because both are technically applicable.
The product owner must choose a capture workflow and target host(s), then select
the smallest format set that satisfies that choice. No material product choice
is inferred by this specification.

## Adapter boundary

The plugin would be a thin host bridge to a reusable asynchronous analysis job;
DSP, measurement units, limits, report serialization, and qualified result
wording remain in the shared analysis layer. Keep host SDK types out of core
headers and keep a no-SDK standalone build. Do not place Qt widgets, file
dialogs, logging, allocation, locking, file I/O, or analysis calls on the host's
real-time audio callback.

The proposed interface and state transitions are in
[plugin-capture-interface.md](plugin-capture-interface.md). It specifies a
non-modifying pass-through effect, bounded capture, worker-thread processing,
explicit dropped-audio provenance, and hard boundaries for channel count,
sample rate, and host blocks. Do not claim equivalence with file analysis until
the same decoded audio and settings produce validated equivalent output.

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
[report integrity](report-integrity.md), and proposed
[capture interface](plugin-capture-interface.md). Before implementation,
stabilize an in-memory or disk-backed cancellable job API, define host and
workflow scope, choose capture limits and incomplete-capture policy, pin the
format SDK/API, and complete dependency and license review. This specification
does not authorize a format choice or assert that the currently shipped
standalone release includes a plugin.
