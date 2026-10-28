# Notes
- Core is UI-free (`include/humtrace`, `src/`); CLI and Qt GUI (`gui/`) only call `analysis.hpp`.
- Build: `make test` (CLI, no deps) or `cmake -G Ninja` (adds Qt GUI + ctest + JUnit).
- Pitfall: modal QMessageBox inside a worker-completion slot hangs headless tests; errors go to the summary label instead.
- Pitfall: keep the QThread pointer local in lambdas; a second `openFile` can replace `thread_` before the first thread's `finished` fires.
- Perf: replacing the recurrence twiddles with a strided table made analysis ~2x SLOWER (13 s -> 24 s on 10 min stereo 48 kHz); reverted. FFT is ~85% of time (gprof). Remaining option: real-FFT packing; not needed for beta.
- mingw with `-std=c++20` lacks `M_PI`; use `std::numbers::pi`.
