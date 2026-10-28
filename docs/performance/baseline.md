# Performance baseline

No reproducible throughput or peak-memory benchmark has been established yet. Do not claim performance targets have been met.

## Baseline plan

Measure decoder throughput, spectrum frames per second, whole-file runtime, peak resident memory, and timeline result size on representative mono/stereo recordings at 44.1/48/96 kHz. Record CPU, RAM, OS, compiler/version, optimization mode, input duration/format, frame size, hop, and wall time. Include a long-file bounded-memory test before selecting streaming architecture.

Use results to compare the current radix-2 FFT against an approved library and to decide whether allocations, SIMD, or threaded frame processing are warranted. Preserve numerical tolerances and test correctness before accepting an optimization.

## Measured baseline (2026-10-28)

Synthetic 10-min 48 kHz 16-bit stereo WAV (115 MB), auto frame 32768, hop 16384, CLI with JSON export. Linux 6.18 container, g++ 13 -O2, single thread.
Wall time 13.1 s (~46x real time); peak RSS ~11 MB; JSON 16 MB. gprof on a 60 s excerpt: `analyze_spectrum` (FFT) 85%, SHA-256 7%, decode 3%. A strided twiddle-table FFT was tried and was slower (24 s); reverted. Not measured: Windows, other rates, GUI rendering.
