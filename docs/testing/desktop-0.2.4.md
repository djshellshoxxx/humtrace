# Desktop 0.2.4 validation evidence

Recorded 2026-10-24 UTC in the Linux x86-64 execution environment, using GCC 14.2.0, Qt 6.8.2, Release builds and the Qt offscreen platform. This records automated checks; native assistive-technology and human acceptance remain separate below.

## Final results

| Check | Result |
|---|---|
| Full CMake build | Passed, maximum two compiler jobs, warnings treated as errors |
| Full CTest suite | 27/27 passed, 13.41 s |
| `app_test_gui` | 33 passed, zero failed/skipped; includes setup/cleanup and both minimum-size data rows |
| GUI settings smoke | 21 randomized steps, 17 completed analyses, zero errors |
| GUI settings extended run | 60 s; 418 randomized steps, 335 completed analyses, zero errors |
| Minimum logical window | Exact 1024×680 in ready/completed states, with default and 30% larger display-control fonts |
| Million-bucket virtual waveform table | 1,048,576 rows; added RSS 8,392,704 bytes, below the 64 MiB test bound beyond existing envelope vectors |

All core and Qt objects were rebuilt after the core API changes. The final waveform-table and layout additions were rebuilt and linked before the results above. No failures remain in these validation commands.

The extended GUI fuzzer randomizes frame/hop, spectrum/Welch mean removal, Welch segment/hop, search band, peak separation, local-floor guard/radius, harmonic tolerance, peak/support thresholds, tracker settings, presentation resolutions, resource limits, palettes and navigation. It exercises analysis, reanalysis, export, finding selection and cancellation. Seeds are recorded when a failure occurs.

## Long-file memory and responsiveness

`tools/validate_paged_gui_memory.py` generated separate one-hour and three-hour 8 kHz mono 16-bit WAV files, analyzed them completely in the real offscreen window, then navigated 64 time positions. Each child had a 1,536 MiB address-space limit. The final repeat used the waveform-table implementation; it ran concurrently with the extended GUI fuzzer, so elapsed times are observations rather than isolated throughput benchmarks.

| Input | Peak RSS | Heartbeats | Maximum heartbeat gap | Harness elapsed | Final frame/row cache | Final image |
|---|---:|---:|---:|---:|---:|---:|
| 1 hour | 30.63 MiB | 2,575 | 45 ms | 26,022 ms | 2,878,208 bytes | 20,992 bytes |
| 3 hours | 30.97 MiB | 5,782 | 50 ms | 58,564 ms | 3,047,936 bytes | 20,992 bytes |

Both runs passed: peak RSS below 192 MiB; three-hour RSS growth at most 16 MiB (observed 0.34 MiB); caches at most 6 MiB; image at most 2 MiB; maximum heartbeat gap at most 1,000 ms. SHA-256 checks verified the source bytes were unchanged. The small fixture also passed through CTest.

These fixtures establish the reported case and bounds; they do not characterize every sample rate, channel count or storage device.

## Regressions and table coverage

Tests first reproduced and then verified corrections for omitted accessible measurement names/units, the false “row width = resolution” statement, missing plotted spectrum-row values, absent detector controls, no-op settings invalidation, custom-window paging estimates, an invented interpolated peak for silence, stale recording digests, late comparison cancellation, shared 50/60 Hz support-count denominators and the minimum-width constraint.

The settings-dialog test changes the controls and verifies the actual worker report snapshot. Comparison tests use the Qt chooser/progress controls, preserve recording A, cancel/close active workers, and cancel deterministically at the queued comparison-completion boundary. Normal comparison completion is also tested.

Automated table alternatives now cover findings, selected-frame spectrum aggregates and peak/harmonic measurements, waveform envelope buckets, and comparison summaries. Accessible text includes measurement labels, units and channel context; unmatched comparison values are explicitly unavailable. Qt accessible table interfaces are checked.

The waveform tab uses `WaveformModel`, a virtual `QAbstractTableModel` borrowing the existing immutable min/max vectors, with no per-cell objects or source-audio reads. It exposes the selected channel, inclusive start/exclusive end sample indices, time bounds, and stored min/max values in normalized full scale. The numerical role preserves the stored values; displayed levels round-trip to the stored float. These are envelope summaries, not individual raw audio samples. The last bucket is clipped using the last analyzed frame's exact start sample plus frame size, including the disk-backed frame index.

Retained/paged parity tests check every bucket, channel changes after deleting the input fixture, empty ready state, exact final partial-bucket bounds, semantic headers and a million existing buckets. This closes the automated waveform-table gap. It does not constitute native screen-reader sign-off.

## Reproduction commands

The workspace-private CMake/Ninja installation and extracted Qt SDK were used as follows; a normal Qt development installation can replace these paths.

```sh
export PYTHONPATH=/workspace/humtrace-tools
export LD_LIBRARY_PATH=/workspace/humtrace-sdk/usr/lib/x86_64-linux-gnu
export QT_QPA_PLATFORM_PLUGIN_PATH=/usr/lib/x86_64-linux-gnu/qt6/plugins
export QT_QPA_PLATFORM=offscreen
export XDG_CACHE_HOME=/workspace/humtrace/build-desktop/cache

/workspace/humtrace-tools/bin/cmake -S . -B build-desktop -G Ninja \
  -DHUMTRACE_BUILD_GUI=ON -DHUMTRACE_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/workspace/humtrace-sdk/usr
/workspace/humtrace-tools/bin/cmake --build build-desktop --target clean --parallel 2
/workspace/humtrace-tools/bin/cmake --build build-desktop --parallel 2
/workspace/humtrace-tools/bin/ctest --test-dir build-desktop --output-on-failure -j1
build-desktop/bin/gui_settings_fuzz 60
python tools/validate_paged_gui_memory.py build-desktop/bin/app_test_paged_memory

mkdir -p build-desktop/screenshots
HUMTRACE_SCREENSHOT_DIR=/workspace/humtrace/build-desktop/screenshots \
  build-desktop/bin/app_test_gui analyzesStereoFileAndShowsPerChannelFindings \
  visualStatesRenderWithoutErrors \
  waveformTablePreservesEnvelopeValuesAndPartialBoundsForBothStorageModes
```

The dependency-free presentation test was also compiled directly with `g++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Werror -Iinclude -Iapp/presentation tests/app/test_presentation.cpp app/presentation/findings.cpp src/music.cpp` and passed.

Local evidence is in `build-desktop/Testing/Temporary/LastTest.log`, `build-desktop/gui-settings-fuzz-60.log`, and `build-desktop/paged-gui-memory.log`. Generated screenshots are build artifacts, not native-theme approval:

- `build-desktop/screenshots/complete.png`: 1440×900.
- `build-desktop/screenshots/narrow.png` and `error-state.png`: 1024×680.
- `build-desktop/screenshots/ready.png`, `linear-grayscale.png`, `zoomed-channel2.png`, `stale.png`: 1440×900.
- `build-desktop/screenshots/waveform-table-{retained,paged}-{bounds,levels}.png`: 1024×680; horizontal scrolling exposes all unit-bearing columns.

## Remaining native and human gates

- Windows Narrator and a Linux screen reader/AT-SPI session on the actual packaged application have not been exercised by this worker. Toolkit accessible roles/data are necessary evidence, not a replacement.
- Human keyboard traversal, focus visibility, native theme/high-contrast review, 100–200% text/UI scaling, and the complete packaged user workflow still require native review. The enlarged-control-font regression simulates layout pressure; it is not native macOS, Windows or high-DPI acceptance.
- The long-file heartbeat bound is separate from cancellation latency. The existing GUI cancellation regression allows up to three seconds; the specification's 250 ms reference-machine cancellation target has not been established by these measurements. Close-during-export retains its separate existing 250 ms automated bound.
- Native CI/package results are tracked by the release coordinator; the offscreen results above do not assert those jobs or human gates passed.
