# Troubleshooting notes

## LeakSanitizer cannot inspect processes in this workspace

**Observed:** default AddressSanitizer runs ended with `LeakSanitizer has encountered a fatal error` and `LeakSanitizer does not work under ptrace` / inability to open `/proc/.../task`.

**Scope:** environment-level leak-check failure. The address and undefined-behavior sanitizers still ran when leak scanning was disabled.

**Workaround used:** set `ASAN_OPTIONS=detect_leaks=0` for sanitizer test runs. This does not verify memory leaks. Repeat LeakSanitizer or an equivalent leak check on a supported native CI runner before beta.

## CMake is not installed in the current environment

The initial project uses a small Makefile and C++20 compiler because `cmake` was unavailable. If the project adopts JUCE or needs native installers, establish and verify a CMake build in CI and the developer environment rather than treating the Makefile as the final cross-platform packaging system.
