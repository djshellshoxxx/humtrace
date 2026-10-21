# Developer Build and Test

## Toolchain

- C++20 compiler (GCC or Clang on Linux)
- `make`
- Windows: MSYS2 (CI uses `msys2/setup-msys2@v2`)
- Python only for `tests/test_cli.py`

Compiler flags come from the Makefile: `-std=c++20 -O2 -Wall -Wextra -Wpedantic -Wconversion -Werror`. Warnings fail the build.

## Build

```sh
make                      # builds build/humtrace
```

The output is `build/humtrace`.

## Test

```sh
make test
```

The `test` target builds and runs `test_spectrum`, `test_wav` and `test_report`. `tests/test_cli.py` runs the CLI end to end.

## CI (`.github/workflows/ci.yml`)

- `ubuntu-latest`: `make`, then `make test`
- `windows-latest`: MSYS2 setup, then `make`

## Release

There is no release workflow yet. See `docs/release/` for the planned process.

## Troubleshooting

- `-Werror` fails on a warning: fix the conversion or unused variable. Do not remove the flag.
- `make` on Windows fails without MSYS2: use the MSYS2 shell the CI workflow uses.
