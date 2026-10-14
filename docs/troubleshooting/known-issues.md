# Troubleshooting notes

## LeakSanitizer cannot inspect processes in this workspace

**Observed:** default AddressSanitizer runs ended with `LeakSanitizer has encountered a fatal error` and `LeakSanitizer does not work under ptrace` / inability to open `/proc/.../task`.

**Scope:** environment-level leak-check failure. The address and undefined-behavior sanitizers still ran when leak scanning was disabled.

**Workaround used:** set `ASAN_OPTIONS=detect_leaks=0` for sanitizer test runs. This does not verify memory leaks. Repeat LeakSanitizer or an equivalent leak check on a supported native CI runner before beta.

## Build systems

The Makefile builds and tests the dependency-free core and CLI (used for sanitizer and fuzz CI jobs). CMake builds everything including the Qt 6 app, GUI tests, and packages. Both are verified on Linux; the CMake/Qt path on Windows (MSYS2 UCRT64) is configured in CI but unverified.

## Offscreen GUI tests

GUI tests run with `QT_QPA_PLATFORM=offscreen`, which prints `This plugin does not support propagateSizeHints()`; the message is harmless. Offscreen rendering does not validate screen-reader behavior, native dialogs, or high-DPI scaling.
