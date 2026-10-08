# Performance baseline

No reproducible throughput or peak-memory benchmark has been established yet. Do not claim performance targets have been met.

## Baseline plan

Measure decoder throughput, spectrum frames per second, whole-file runtime, peak resident memory, and timeline result size on representative mono/stereo recordings at 44.1/48/96 kHz. Record CPU, RAM, OS, compiler/version, optimization mode, input duration/format, frame size, hop, and wall time. Include a long-file bounded-memory test before selecting streaming architecture.

Use results to compare the current radix-2 FFT against an approved library and to decide whether allocations, SIMD, or threaded frame processing are warranted. Preserve numerical tolerances and test correctness before accepting an optimization.
