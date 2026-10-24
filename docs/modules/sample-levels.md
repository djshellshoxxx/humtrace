# Raw sample levels

`measure_sample_levels` computes unwindowed signed mean, RMS, and maximum absolute sample amplitude from one decoded channel. It rejects empty and non-finite input and does not alter samples. Normalizing by the sample peak before summing squares avoids overflow and underflow; RMS is evaluated in the log domain.

RMS includes DC. The reference amplitude is 1.0, so a full-scale sine reads -3.0103 dBFS RMS; a constant amplitude 0.5 reads -6.0206 dBFS. Sample peak is not an intersample true peak. Reporting floor is -300 dBFS, with separate boolean flags when RMS or peak falls below it. Silence returns finite floor values and both flags set.

The streaming pipeline and in-memory interference timeline attach these measurements to each frame before DC removal and windowing. Overlapping frames are independent measurements, not a whole-file aggregate. `measure_frame`, which only receives a precomputed spectrum, leaves raw sample levels unavailable. JSON exports this explicitly as null when absent. The desktop finding inspector shows the frame RMS range and maximum sample peak across each track's frames.

Tests use analytical sine/DC, silence, subnormal-scale samples, large finite samples, invalid input, pipeline measurements, JSON schema validation, and desktop presentation assertions.
