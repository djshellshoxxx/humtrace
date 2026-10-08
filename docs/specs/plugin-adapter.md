# Deferred feature specification: plugin adapters

**Release status:** outside beta v0.1. A plugin is an optional client of the analysis library; the desktop app and CLI remain independently useful.

## Host model and scope

First define the plugin use case before selecting a format. Candidate workflow is offline analysis of a host-provided audio stream, not real-time monitoring or audio modification. The plug-in must not block the audio callback, allocate unbounded memory, open file dialogs from a callback, or claim evidence equivalence to the standalone file reader unless both paths are validated against the same fixtures.

## Adapter boundary

- Core measurement types and settings are standard C++ and GUI/host independent.
- A plug-in adapter maps host channels/sample rate/transport blocks to the shared analysis job API and reports supported constraints.
- If analysis requires a multi-second window, buffer only to a declared bounded capacity and expose latency/memory; allow offline render or explicit analysis rather than pretending to be zero-latency.
- Host automation controls only documented analysis parameters; changes invalidate previous results and are captured in the report.
- Never modify input audio in the first adapter. Any monitoring output is a separate opt-in path with explicit latency and no feedback-loop assumptions.

## Format-selection gate

Before implementation compare CLAP, VST3, and any requested formats by host support, SDK terms, distribution obligations, build/test coverage, and user workflow. Pin SDK versions, record licenses/notices, and preserve a no-plugin core build. Do not bundle a plugin SDK into the core. The product owner must define target hosts and whether plugin use is required before this work starts.

## Verification

Use host-validation tools where available plus real-host tests for lifecycle, sample-rate changes, varying block sizes, mono/stereo/multichannel layouts, reset, suspend/resume, transport stop, offline render, automation, state save/restore, and cancellation. Compare plugin and standalone outputs numerically on identical decoded input/settings. Run plug-in validation and signing/package smoke tests independently for every claimed format/host.

## Reuse and prerequisites

Implement one thin adapter per plugin format after the analysis/report API stabilizes. Reuse [analysis engine](analysis-engine.md), [long-file workflow](long-file-workflow.md), and [report integrity](report-integrity.md). No plugin framework belongs in DSP headers or the dependency-free CLI target.
