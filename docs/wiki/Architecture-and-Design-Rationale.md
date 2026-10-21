# Architecture and Design Rationale

Sources: `docs/architecture/decisions.md`, `docs/architecture/overview.md`, README.

- **D-001: measurement modules independent of the GUI.** WAV reading, spectrum analysis, result structures and report serialisation do not depend on GUI state or JUCE. The CLI, a future desktop UI and plugins need the same analysis (decision log).
- **D-002: levels reported in dBFS.** Peak amplitude from normalised samples; not SPL or voltage (README).
- **Bounded streaming.** The CLI streams WAV audio through bounded rolling windows, but still keeps the timeline and JSON report in memory. `decode_wav` is an in-memory wrapper (README).
- **Frames are independent.** The timeline reports each frame on its own; it does not link peaks into tracks or compare recordings (README).
- **Warnings as configuration.** The build uses `-Werror` with strict warnings, so conversions and pedantic issues fail the build.
