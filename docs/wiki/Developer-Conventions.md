# Developer Conventions

## Style

- C++20. Use the standard library only.
- Headers in `include/humtrace/`, implementation in `src/`. Keep the public types in headers.
- Warnings are errors. Fix them. Do not add suppression flags to the Makefile.
- Mark return values `[[nodiscard]]` where a dropped result is a bug (the existing headers do this).

## Measurement rules

- Levels are peak dBFS on normalised samples. Do not convert them to SPL or voltage in the code without a calibration design (see `docs/research/dsp-algorithms.md`).
- Observed energy and source attribution stay separate in the data model. Do not add a field that names a source.
- A mains match is not a diagnosis. Keep the report wording that says so.

## Reports

- The JSON report has a `schema_version`. Any field change needs the schema file, the docs and the tests updated together.
- The report does not yet carry input hashes or decoder versions. Adding them is a planned change, so do it in a new schema version.

## Gotchas

- `analyze_spectrum` needs a power-of-two sample count. The CLI handles this. Callers using the library must do the same.
- `decode_wav` keeps all samples. Use `WavStreamReader` for long files.
- The timeline and JSON stay in memory. Memory grows with duration.
- The FFT is hand written. Test any change against the existing `test_spectrum.cpp` cases.

## How to add a feature

1. Add the data type to the right header.
2. Implement it in the matching `src/*.cpp`. Keep `wav`, `spectrum` and `report` separate.
3. Add a test file or cases in `tests/` and add it to the Makefile `test` rule.
4. Wire it into `src/main.cpp` if it is user-facing.
5. Update the JSON schema and docs if the report changes.

## Known limitations

From `docs/user-guide/limitations.md`:

- Only RIFF/WAVE with the listed formats.
- Memory grows with recording length in the CLI.
- No validated detector. Per-frame peaks are not linked into tracks.
- No GUI, no ENF matching, no codec support, no plugin.
