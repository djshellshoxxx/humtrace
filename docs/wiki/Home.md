# HumTrace Wiki

HumTrace is a Circuit Drift Labs audio interference analysis tool. It is an offline C++20 command-line prototype. It reads a WAV file, looks at the spectrum frame by frame, and reports how strong the 50 Hz and 60 Hz mains harmonics are. It writes the results to JSON.

**Status:** engineering prototype, not beta. The measurements are useful for looking, not for proving anything. A match to mains harmonics does not show a ground loop, a device, a place or a recording time.

**Who it is for:** engineers who want to see mains-related interference in a recording, and developers who want to know how the analysis is built.

## Quick start

```sh
make
make test
build/humtrace recording.wav
build/humtrace recording.wav --json analysis.json
```

Needs a C++20 compiler and `make`. Windows builds run under MSYS2 in CI.

## Pages

**For users**
- [[User-Guide]]: what the tool reads, what it prints, how to read the numbers, and limits

**For developers**
- [[Developer-Overview]]: pipeline, design decisions, data flow
- [[Developer-Code-Map]]: files, public types and functions, where to change what
- [[Developer-Build-and-Test]]: build, tests, CI
- [[Developer-Conventions]]: strict warnings, rules, gotchas

**Repo docs (authoritative)**
- `docs/architecture/overview.md` and `decisions.md`
- `docs/research/dsp-algorithms.md`, `docs/research/gui-toolkit-decision-research.md`
- `docs/specs/product-requirements.md`, `docs/specs/gui.md`
- `docs/modules/report-schema-v1.md` and `schema/analysis-report-v1.schema.json`
- `docs/user-guide/limitations.md`
