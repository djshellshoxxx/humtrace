# Codebase Map

| I want to... | Look in |
|---|---|
| WAV reading | `include/humtrace/wav.hpp`, `src/wav.cpp` |
| Spectrum and frame analysis | `include/humtrace/spectrum.hpp`, `src/spectrum.cpp` |
| Report structures and JSON | `include/humtrace/report.hpp`, `src/report.cpp` |
| Command-line front end | `src/main.cpp` |
| Tests | `tests/` (`test_wav.cpp`, `test_spectrum.cpp`, `test_report.cpp`, `test_cli.py`) |
| Build | `Makefile`, `.github/workflows/ci.yml` |
| Design, architecture, research, release | `docs/` |
