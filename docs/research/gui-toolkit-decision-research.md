# HumTrace GUI toolkit decision research

**Reviewed:** 2026-10-08  
**Scope:** Desktop GUI for Windows and Linux; offline audio inspection; waveform, spectrum, spectrogram, finding timeline, accessible results, packaging, and testability.  
**Recommendation:** Use **Qt 6 Widgets with C++ and CMake** for the first desktop application, subject to the prototype gates below. This is a toolkit recommendation, not a decision about HumTrace's own license.

## Executive decision

Qt Widgets is the best fit for HumTrace's planned desktop workflow. HumTrace is an offline measurement and evidence-inspection application with dense controls, tables, plots, file workflows, and a need for keyboard and screen-reader use. Qt Widgets supplies the general application shell and standard controls; its `QPainter` system can draw the custom audio views; Qt exposes accessibility interfaces for custom widgets; Qt Test covers application-level widget interactions; and Qt documents Windows and Linux packaging paths. Qt's documented CMake workflow also fits the repository's C++ build direction. [1][2][3][4][5]

This recommendation does **not** select or change HumTrace's product license. It assumes the project is willing to satisfy the applicable terms for every Qt module and bundled dependency it uses. Qt documents LGPLv3, GPLv3 and commercial options, and specifically lists Qt Graphs among its GPLv3 modules under the open-source licensing option. Keep the initial GUI to the Qt Core, Gui, Widgets, and (development/test target only) Test modules after checking the precise version's module license/SBOM. Use QPainter-based custom plot widgets instead of Qt Graphs, Qt Charts, or Quick/QML add-ons. If a future product license or distribution plan cannot accommodate the selected Qt terms, reopen this decision before shipping. [6][7]

The decision remains gated on a small native Windows/Linux prototype. The toolkit provides accessibility and rendering mechanisms; it cannot ensure the custom timeline, waveform, or spectrogram is accessible or performant without HumTrace-specific implementation and validation.

## Why Qt Widgets fits this application

### Product shape

HumTrace is an analysis workstation rather than a live audio instrument. It needs reliable menus, file dialogs, tables, keyboard navigation, selection and inspection, settings forms, a report preview, progress and cancellation, and custom visualizations. The existing architecture already separates audio/DSP/report modules from the future app and explicitly forbids GUI state from entering the analysis core. The GUI should therefore remain a thin C++ desktop client that invokes the core and displays immutable results. [8][9]

### Target support and build workflow

Qt 6.12's current supported-platform matrix includes Windows 10 (1809 or later) and Windows 11 on x86-64, with MSVC 2022 or MinGW-w64; it also identifies specific supported Linux configurations. Qt notes that support for particular OS configurations can change in patch releases, so HumTrace must pin a supported Qt baseline and test its own named OS images instead of treating “Linux” as one universal binary target. **Qt 6.12 is the last release planned to support Windows 10.** If HumTrace beta promises Windows 10, the team must decide whether to keep a 6.12 line for it or move the supported OS floor when upgrading Qt; this should be an explicit support policy, not an accidental result of a toolkit update. Qt's official Linux binaries are built on Ubuntu 24.04/glibc 2.39; the Linux support page says older glibc targets require a source build. That makes Linux ABI selection an explicit packaging decision. [1][10]

Qt's official CMake guidance uses `find_package(Qt6 REQUIRED COMPONENTS Widgets)`, `qt_standard_project_setup()`, `qt_add_executable()`, and `target_link_libraries(... Qt6::Widgets)`. This is a direct fit for introducing the GUI as its own app target while retaining current core tests and CLI targets. Use a separate `humtrace_gui` target and avoid adding Qt headers or types to `include/humtrace`. [3]

### Rendering, style, and high-DPI behavior

Qt Widgets supports custom `QWidget` subclasses and painting through `QPainter`. Qt describes its paint functions as optimized and provides clipping, paths, text, images, transforms, and separate paint devices. Its high-DPI system scales Qt Widgets automatically; QPainter uses the backing-store device-pixel ratio. These are sufficient primitives for HumTrace's custom views without introducing a graphing framework dependency. [4][11]

Use native Qt Widgets for controls and tables, then apply a restrained HumTrace palette and spacing scale. This yields familiar focus, selection, menus, dialogs, and text-entry behavior while allowing a distinctive dark analysis canvas. The GUI should use a built-in style as its base and customize palette and selected control details; Qt documents native platform styles and the Fusion style within Qt Widgets. Avoid styling every control as a fully custom-drawn object because that would add accessibility and maintenance work. [12]

### Accessibility

Qt's accessibility layer communicates with assistive technologies such as screen readers and braille displays. Its documented platform support includes Microsoft Active Accessibility (MSAA) and Unix/X11 AT-SPI; built-in Qt Widgets have accessibility implementations, and custom widgets can supply `QAccessibleInterface` or derive from `QAccessibleWidget`. Qt's interface also exposes actions, values, tables, selection, and (in 6.12) viewport interfaces. This is a stronger baseline for HumTrace than a UI framework that renders all controls in a custom canvas, but it is not proof of compliance or assistive-technology quality. [5]

Do not make the charts the only representation of their data. Pair each plot with a keyboard-reachable measurement table or event list, expose concise accessible names/descriptions and values for selected items, and provide keyboard equivalents for selection, zoom, navigation, and channel changes. Screen-reader tests on supported Windows and Linux configurations are a release gate. Qt documentation notes an AT-SPI enablement condition on Linux/X11; test the actual target desktop and session protocol rather than assuming accessibility is active merely because Qt supports the API. [5]

### Tests and automation

Qt Test supports data-driven testing, mouse and keyboard simulation, benchmarking, and CMake/CTest integration. Use it for widget behavior and application orchestration; continue using the current dependency-free tests for DSP and report correctness. The project's acceptance remains about HumTrace behavior: e.g. a selected finding moves the shared playhead, chart zoom does not rerun analysis, analysis cancellation stops work, and export writes the currently previewed report. Framework tests alone cannot validate color contrast, screen-reader announcements, packaging, or the scientific correctness of plotted data. [2]

### Distribution

Qt's Windows deployment documentation recommends `windeployqt` to assemble the Qt libraries, plugins, and runtime dependencies into a deployment tree for an installer. Qt's Linux documentation describes CMake deployment APIs (available from Qt 6.5) and DEB packaging, while warning that Linux deployment must account for the runtime libraries and plugin paths. This gives a clear route to Windows installer contents and Linux package artifacts, but HumTrace still needs to select its supported Linux baseline and verify clean-machine installs. [13][14]

For the initial beta, ship a Windows x64 installer and a Linux package for one explicitly named baseline (recommend Ubuntu LTS x86-64 first). Add broader distribution formats only after compatibility tests. Build Qt and HumTrace from a consistent supported toolchain; do not claim every Linux distribution is supported from a single CI job. Preserve Qt's notices and license texts, produce a dependency/SBOM manifest, and record the Qt version, compiler, modules, deployment output, and package hash in release metadata. The exact LGPL obligations depend on the modules and distribution method and must be reviewed before the first public binary. [6][7][10][13][14]

## Alternative comparison in the context of HumTrace

The existing [GUI toolkit evaluation](gui-toolkit-evaluation.md) documents the broader candidate set and sources. This report narrows the recommendation to the application's current shape and updates the Qt licensing/accessibility details from the current Qt 6.12 docs.

| Toolkit | Fit for this app | Main tradeoff |
|---|---|---|
| **Qt 6 Widgets — recommended** | Strong standard desktop workflow; custom plots through QPainter; documented accessibility interfaces, tests, CMake, and platform packaging. | Need precise per-module license/SBOM review; custom graphs need bespoke accessibility and performance work; Linux support must be defined by baseline. |
| **JUCE** | Excellent audio-oriented APIs and custom drawing; appropriate if the product later becomes a live audio host/plugin or needs device/audio callback features. | HumTrace v0.1 is offline analysis, so most audio-host functionality is not an advantage; JUCE 9 has AGPLv3/commercial licensing paths to evaluate, and the measurement core deliberately avoids framework coupling. [16][17] |
| **wxWidgets** | C++ desktop toolkit with platform-native control approach and a permissive distribution path under its wxWindows Library Licence. | Less direct fit for highly custom analytical plots and uneven confidence about custom-widget screen-reader behavior across target Linux sessions; would still need custom chart accessibility and test infrastructure. [18][19] |
| **SDL2 + Dear ImGui** | Fast route to a custom technical dashboard; compact custom-rendered widgets and backend flexibility. | The interface is custom-rendered and needs more application-shell, keyboard, screen-reader, and accessibility engineering; this is a poor trade for an evidence-inspection tool whose findings must be independently navigable. [20][21] |

JUCE's unique advantage is its audio stack, but the first GUI does not need to open an audio device or run DSP in a real-time callback. The useful shared audio file and DSP capabilities must not pull framework APIs into the core. If HumTrace later adds live capture or a plugin product, reevaluate the app adapter then rather than couple the current offline app to a plugin framework now. This is an architectural judgment based on the current requirements and module boundaries. [8][9]

## Recommended GUI structure and appearance

This section refines the existing [desktop GUI specification](../specs/gui.md). It is a layout and interaction specification for Qt Widgets; it does not replace the report schema or DSP contracts.

### Visual direction

- **Overall feel:** precise, calm measurement instrument. Keep the shared Circuit Drift Labs technical identity subtle. Avoid a game-like dashboard, animated decoration, neon overload, or alarm styling for measurements that are not confirmed hazards.
- **Canvas:** near-black blue-gray background; slightly lighter panel surfaces; high-contrast text; thin low-contrast grid; one saturated cool accent for selected/active state. Event categories may use restrained colors, but also include labels, glyphs, or line patterns.
- **Typography:** system UI font for controls and body text; tabular numerals for aligned measurements if available. Frequency, level, duration, and thresholds use visible units (`Hz`, `dBFS`, `s`). Never rely on tiny tick labels.
- **Density:** a compact, resizable three-pane analysis workspace. Keep primary actions in a top toolbar, results navigation on the left, plots in the center, and evidence detail on the right. Provide splitter handles and a reset-layout command; do not force a fixed pixel layout.
- **Honesty:** distinguish measured values, method/settings, heuristic flags, and possible interpretations by section labels. The interface must not turn a supported 50/60 Hz harmonic pattern into an asserted ground loop, device, room, or location.

### Window and component specification

| Component | Qt Widgets implementation | Required content and behavior |
|---|---|---|
| Main window | `QMainWindow`, menu bar, toolbar, status bar, dockable or splitter-based panes | Open, Analyze, Cancel, Export, Compare, Settings, Help; current operation; keyboard shortcuts; keep analysis responsive. |
| File/session navigator | `QDockWidget` or left `QWidget` in `QSplitter`; `QTreeView`/model where multi-session items exist | Current recording, recent sessions, channels, decode state and format metadata; do not modify original file. |
| Analysis canvas | Central `QWidget` with vertical splitter and plot widgets | Overview waveform, spectrogram, optional spectrum, shared time selection and playhead. Plot values derive from immutable analysis/display data models. |
| Waveform view | Custom `QWidget` + `QPainter`; sample-envelope pyramid/model built off the GUI thread | Overview amplitude envelope per visible channel; independent channel rows; time range selection and navigation; downsample for display only. Never imply that display decimation changed analysis data. |
| Spectrogram view | Custom `QWidget` rendering precomputed display tiles/`QImage` rows; reusable color map | Time/frequency axes, log/linear scale control if supported by data model, frequency range, color scale legend, selected range, channel visibility. Color map must be perceptually ordered and accompanied by numeric/text access. |
| Spectrum view | Custom `QWidget` with `QPainter` axes/grid/trace and cursor overlay | One selected frame/range, frequency and dBFS cursor readout, bin resolution, window/hop metadata, peak labels. Provide a tabular “peaks/measurements” equivalent. |
| Timeline | Custom view or `QTableView` backed by event model, with shared time transform | Persistent findings only when tracking is implemented; until then label the current frame measurements accurately. Keyboard-selectable event rows and visible marker labels; selecting an event updates shared cursor/context. |
| Finding list | `QTableView` + `QAbstractTableModel` | Start, end, channel, measured frequency, peak dBFS, persistence, evidence flags, analysis method; sortable by explicit columns. Do not sort on a hidden confidence score. |
| Finding inspector | `QWidget`/form layout with sections and copyable text | Measurement, harmonics and local-floor values, frame/hop, evidence summary, cautious interpretation, and limitations. Include copy measurements action. |
| Progress/cancel | `QProgressBar`, status text, `QPushButton`; worker/job controller outside plot widgets | Determinate progress when computable, indeterminate otherwise; current phase/channel; cancellation flag checked by decoder and analysis loops. No modal dialog that blocks inspection. |
| Report preview | `QDialog` with summary plus schema/version/settings/warnings and path chooser | Show what will be exported, destination, overwrite confirmation, and report limitations; atomic write behavior belongs to report/file service, not widget code. |
| Settings | `QDialog`/`QTabWidget` with normal labels, controls, contextual help | Frame, hop, thresholds, display-only scale, default directories, appearance; distinguish analysis settings from view settings and require rerun for analysis changes. |

### Layout at launch

At a typical 1440×900 desktop size, use a 48–56 logical-pixel toolbar; a left rail around 220–280 logical pixels; a right inspector around 300–360 logical pixels; and reserve the center for a waveform overview plus spectrogram. These are starting targets, not fixed constraints. At smaller windows, allow the navigator and inspector to collapse into tabs/docks while keeping plots and the event list accessible. At high DPI, size geometry in logical units and test at 100%, 150%, and 200% scaling. Qt documents high-DPI scaling in its Widgets and QPainter paths; all actual layouts and plot labels still require visual validation. [15]

Preferred main view order:

1. Toolbar and current-file identity (`name`, duration, sample rate, channels, encoding).
2. Full-recording waveform overview with selection/playhead.
3. Spectrogram for time-frequency structure.
4. Optional spectrum for the currently selected frame/range.
5. Findings table/list plus the inspector when a result is selected.

If the plot canvas is height-constrained, allow the user to hide/reorder spectrum and spectrogram panels, but keep a persistent selection summary and explicit focus indication. Do not hide measurements solely in hover tooltips.

### Interaction contracts

- **One time model:** use a shared mapping between sample index, seconds, plot x-coordinate, and timeline row. Store selected ranges in sample/time units, never in screen pixels.
- **Selection:** mouse drag selects a time range. Keyboard supports home/end, arrows with documented step sizes, shift+arrows to extend, plus shortcuts for previous/next finding and zoom controls. Selection, playhead, and viewport are distinct states and have distinct visuals.
- **Channel handling:** each channel remains independent for detection and visible as a named/numbered row. Hiding a display channel is not a downmix and does not change stored results.
- **Zoom and pan:** change only presentation. View settings must not mutate analysis parameters or produce a silent analysis rerun.
- **Long-file handling:** the UI displays thumbnails/tiles and only visible ranges; it does not copy the complete audio file or full sample vector into a widget. The core currently has report timeline memory growth; the GUI must not worsen this by duplicating full results unnecessarily. This is a current architecture limitation and a future bounded-result/streaming-report task. [8]
- **Interpretation copy:** any candidate text includes the actual measured evidence and a qualifier. No definitive source attribution or location claim from frequency alone.
- **Errors:** show recoverable file/decode/analysis/export errors in a non-blocking banner with exact failure and action. Keep existing plot/session state if an export fails.

## Implementation boundary and module plan

Recommended application-only layering:

1. `app/qt/main.cpp`: application setup, Qt metadata/resources, global theme and top-level error handler.
2. `app/qt/MainWindow`: menu/toolbar, pane layout, commands and session state.
3. `app/application/AnalysisController`: starts/cancels jobs, sends progress/result values, owns worker lifecycle. No widgets or `QObject` references enter DSP/core modules.
4. `app/application/SessionModel`: active input metadata, channel visibility, view range, selected event and display-only preferences.
5. `app/presentation/models`: Qt list/table models adapted from immutable domain results.
6. `app/qt/plots`: waveform, spectrogram, spectrum, and timeline widgets. Each consumes display models or immutable result snapshots; no file parsing, FFT, peak finding, or report mutation in paint events.
7. Existing `humtrace_core`: remains standard C++ and independently buildable/testable. Add a GUI-specific target linked to this library; do not link core tests against Qt.

Use one worker per analysis job, with cancellation checked at defined batch/frame boundaries. In Qt, worker objects can be moved to a `QThread` or work can be scheduled through Qt's thread pool, but controller ownership and cleanup must be explicit. Send progress and completed immutable result snapshots back through queued signals. Never call QWidget methods from a worker thread. Do not create a worker per plot or per audio frame. Because current `WavStreamReader`/analysis interfaces do not expose progress or cancellation, implement those core contracts before promising responsive cancel for large files.

## Licensing and dependency constraints

Qt's current licensing page says Qt applications can use its commercial license or open-source licenses when their conditions are met; it identifies specific modules available under GPLv3 and lists Qt Graphs there. Qt docs also identify Qt tools/utilities as separately available under commercial terms or GPLv3 with the Qt GPL exception. Therefore:

- Do not add Qt Graphs, Qt Quick 3D, or other GPL-only modules to the first application.
- Use basic Qt Core/Gui/Widgets capabilities and QPainter custom widgets; verify the exact versions of every module and transitive third-party component in the Qt SBOM before pinning.
- Keep Qt Test out of release binaries if it is used only for development; audit it as a development dependency too.
- Keep build tools and runtime libraries separate in notices and the dependency manifest.
- Prefer dynamic Qt linking and include required notices, license text, and replacement/relink information as required by the selected license and actual shipping method. Confirm the exact obligations against authoritative license text before distribution.
- Do not select a HumTrace license in this toolkit decision. A future product-license decision must be checked against the project's own preferences and the chosen Qt modules before release.

Qt's licensing docs explicitly caution that individual modules can differ and that third-party code has separate licenses. This is why “Qt is LGPL” is not an adequate license record for the project. [6][7]

## Prototype and acceptance gates

Build a short-lived prototype before committing the toolkit dependency broadly. Use one known stereo WAV fixture and the current report model.

### Functional prototype

- CMake builds the GUI on Windows x64/MSVC and Linux x86-64 against one explicitly pinned Qt 6 release.
- Open WAV, show file metadata, launch background analysis, cancel it, and show a completed result without blocking the event loop.
- Draw waveform, spectrogram placeholder driven from a deterministic fixture, spectrum cursor readout, and event table.
- Selecting an event updates the same sample/time selection in every view. Zoom and pan do not change analysis output.
- Report preview and export call existing report code; GUI has no duplicate serializer.

### Accessibility prototype

- Use Windows Narrator with MSAA bridge and Linux Orca with AT-SPI on the supported desktop session.
- Complete open → analyze → navigate findings → inspect textual measurements → export using keyboard only.
- Confirm focus order, accessible names/roles/values, announcement of progress/completion/errors, selected event, channel labels, and chart summary/table alternatives.
- Ensure all status and event meaning is conveyed by label/text/pattern as well as hue; verify high contrast palette and 150%/200% scaling.
- Confirm charts do not become a long, unhelpful list of every pixel or FFT bin; expose semantic summaries and a usable data view.

### Performance and packaging prototype

- Create synthetic files at 1-minute and 1-hour durations, mono and stereo, with deterministic tones/noise. Measure startup, first paint, plot pan/zoom, peak memory, full-analysis duration, cancellation latency, and UI frame responsiveness on a documented reference PC.
- Require no GUI freeze during analysis; establish a measured cancellation target (initial goal: under 1 second after a batch/frame checkpoint) and p95 interaction target (initial goal: under 100 ms) on reference hardware. Treat these as prototype goals, then revise from measurements.
- Test waveform/spectrogram drawing with large files and high-DPI scaling; paint only visible areas and cache view-only tiles without treating them as analysis results.
- Build Windows deployment tree with `windeployqt`, package it, then install/run on a clean Windows VM without the Qt SDK installed.
- Build the named Linux package using the chosen baseline; install/run on a clean VM/container with only documented system prerequisites. Record unresolved portability boundaries.
- Generate and inspect the exact module/dependency inventory and SBOM; reject the prototype if it pulls in an unintended GPL-only runtime module or leaves untracked redistributables.

**Adopt Qt Widgets** if these gates pass and the final distribution plan can satisfy module terms. If accessibility or Linux packaging fails, repair the narrow gap or compare wxWidgets/JUCE again before adding more UI. If a licensing incompatibility is identified, pause toolkit adoption and reassess against the product license rather than changing that license implicitly.

## Known technical risks and mitigations

| Risk | Mitigation |
|---|---|
| Custom graph widgets are inaccessible by default | Pair plots with tables/event list, implement semantic accessible interfaces, test with actual screen readers on each platform. |
| Qt look differs across Linux distributions | Use built-in platform-aware control style with limited palette customization; test supported desktop/session combinations. |
| Linux binaries depend on a newer glibc or plugin path | Define the Linux baseline explicitly; build there or older; run clean-image package tests; document system dependencies. |
| Plot drawing becomes slow with long recordings | Build min/max waveform pyramids and spectrogram tiles off-thread; cache only derived display data; clip to viewport; profile early. |
| GUI crashes or races during cancellation/close | Make one controller own job state, use cancellation token, join/stop worker on close with bounded UI behavior, and test close/cancel races. |
| Toolkit dependency causes core portability regression | Keep app target isolated; retain no-Qt core/CLI build and CI target. |
| License terms change or a module brings additional obligations | Pin exact Qt release/modules; audit SPDX SBOM and bundled dependencies at each release; rerun license review before binary distribution. |
| GUI presents prototype heuristics as validated findings | Use measurement/evidence/candidate separation and carry limitations from the core into every result view and report preview. |

## Sources

Official Qt documentation and project sources, accessed 2026-10-08. Use the docs for the pinned Qt version when implementing; these URLs may track the current 6.x documentation.

1. Qt, [Qt 6.12 Supported Platforms](https://doc.qt.io/qt-6/supported-platforms.html) and [Qt for Linux](https://doc.qt.io/qt-6/linux.html).
2. Qt, [Qt Test Overview](https://doc.qt.io/qt-6/qtest-overview.html).
3. Qt, [Getting Started with CMake](https://doc.qt.io/qt-6/cmake-get-started.html).
4. Qt, [QPainter](https://doc.qt.io/qt-6/qpainter.html), [Qt Widgets](https://doc.qt.io/qt-6/qtwidgets-index.html), and [High DPI](https://doc.qt.io/qt-6/highdpi.html).
5. Qt, [QAccessible](https://doc.qt.io/qt-6/qaccessible.html) and [Accessibility for QWidget Applications](https://doc.qt.io/qt-6/accessible-qwidget.html).
6. Qt, [Qt 6.12 Licensing](https://doc.qt.io/qt-6/licensing.html).
7. Qt, [Open-Source LGPL Obligations](https://www.qt.io/development/open-source-lgpl-obligations) and [Qt SBOM](https://doc.qt.io/qt-6/sbom.html).
8. HumTrace, [Architecture Overview](../architecture/overview.md).
9. HumTrace, [GUI Specification](../specs/gui.md) and [Product Requirements](../specs/product-requirements.md).
10. Qt, [Qt 6.12 Linux Supported Configurations](https://doc.qt.io/qt-6/linux.html).
11. Qt, [Qt Paint System](https://doc.qt.io/qt-6/paintsystem.html) and [High DPI](https://doc.qt.io/qt-6/highdpi.html).
12. Qt, [Qt Widgets Styling](https://doc.qt.io/qt-6/qwidget-styling.html).
13. Qt, [Windows Deployment](https://doc.qt.io/qt-6/windows-deployment.html).
14. Qt, [Linux Deployment](https://doc.qt.io/qt-6/linux-deployment.html) and [Application Deployment](https://doc.qt.io/qt-6/deployment.html).
15. Qt, [High DPI](https://doc.qt.io/qt-6/highdpi.html) and [Scalability](https://doc.qt.io/qt-6/scalability.html).
16. JUCE, [Features](https://juce.com/features/) and [JUCE 9 License](https://juce.com/legal/juce-9-licence/).
17. JUCE, [CMake API](https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md).
18. wxWidgets, [Overview](https://wxwidgets.org/about/), [Accessibility tutorial](https://wxwidgets.org/docs/tutorials/accessibility/), and [Licence](https://wxwidgets.org/about/licence/).
19. wxWidgets, [CMake sample](https://github.com/wxWidgets/wxWidgets/blob/master/samples/minimal/CMakeLists.txt).
20. SDL, [SDL2 platforms](https://wiki.libsdl.org/SDL2/README-platforms); Dear ImGui, [Backends](https://github.com/ocornut/imgui/blob/master/docs/BACKENDS.md).
21. Dear ImGui, [MIT license](https://github.com/ocornut/imgui/blob/master/LICENSE.txt) and [accessibility request](https://github.com/ocornut/imgui/issues/8022).
