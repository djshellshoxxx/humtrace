# Developer Guide

```sh
make          # builds build/humtrace with -Wall -Wextra -Wpedantic -Wconversion -Werror
make test     # spectrum, WAV, report tests and the CLI integration test
```

Verified on this snapshot: `make test` passes (spectrum, WAV, report and CLI integration tests). CI: `.github/workflows/ci.yml`.

Layout: `include/humtrace/` (headers: `wav.hpp`, `spectrum.hpp`, `report.hpp`), `src/` (implementations and `main.cpp`), `tests/` (`test_wav.cpp`, `test_spectrum.cpp`, `test_report.cpp`, `test_cli.py`).

Keep the measurement modules free of GUI and JUCE dependencies (decision D-001 in `docs/architecture/decisions.md`).
