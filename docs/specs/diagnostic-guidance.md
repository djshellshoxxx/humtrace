# Deferred feature specification: qualified troubleshooting guidance

**Release status:** excluded from beta v0.1. The feature must not turn a measured spectrum into an automatic repair command or source diagnosis.

## Product behavior

Provide an optional “possible causes and next measurements” panel linked to explicit observed evidence. Every item includes: observation that triggered it, alternative explanations, an uncertainty statement, low-risk discriminating test, and source/date for the technical reference. The panel is informational; user decides and records any test. No advice to disconnect protective earth/ground, defeat isolation, open energized equipment, or perform unqualified hazardous electrical work.

## Guidance model

- Rules are versioned data, not embedded branches in DSP.
- A rule references measurable predicates (e.g. stable low-frequency line in one channel, harmonic family support, intermittent broad rise) and does not infer an appliance by name.
- Each candidate guidance item lists evidence for, evidence missing, plausible alternatives, and an instrument or controlled test that can discriminate mechanisms.
- Users can dismiss or annotate advice; this does not change measurement results.
- Guidance database is local, signed/versioned if later distributed, and contains citations plus review date. No telemetry or automatic internet lookup.

## Required validation and governance

Have audio/electrical practitioners review every rule. Use scenario tests where the same acoustic signature arises from different physical sources; verify the UI retains competing explanations. Measure how often guidance suggests unsafe, irrelevant, or overconfident actions. No source-specific guidance ships until there is an auditable mapping from validated observation to the test recommendation.

## Sources and design basis

- [Interference taxonomy](../research/interference-taxonomy.md) explains conductive, inductive, capacitive, radiated, acoustic, sampling, and processing alternatives.
- [Forensic limitations](../research/forensic-limitations.md) defines the evidence hierarchy and source-attribution boundaries.
- Any electrical safety instructions require current authoritative standards and qualified review; audio literature alone is insufficient.
