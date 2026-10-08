# Deferred feature specification: reference signature library

**Release status:** excluded from beta v0.1. A signature is a reproducible measurement descriptor, not a device/location identity.

## Intended use

Allow an analyst to compare a new measured event/track against a user-curated, consented reference collection and retrieve similar descriptors for manual review. Default output is ranked descriptive similarity with the feature dimensions and limitations shown. Never label a device, model, room, person, or location as identified based only on a match score.

## Reference record

Store opaque record ID, creator/organization, consent and license, acquisition date (user-supplied and explicitly unverified), equipment/capture-chain notes, original-file hash if permitted, feature schema/version, channel/rate metadata, settings, derived descriptors, annotation basis, and provenance notes. Keep identifying metadata optional and access-controlled. Do not store original audio unless explicitly enabled and governed.

## Comparison method

Candidate descriptors may include measured frequency distributions, drift, harmonic ratios/levels, bandwidth, modulation, event intervals, and cross-channel relationships. Normalize only using a named method. Report per-feature distances and missing values; do not collapse to an unexplained probability. A future classifier requires known-source data, train/test separation by physical unit and session, calibration, open-set rejection, false-match/nonmatch rates, and external validation.

## Governance and tests

Version the schema; support export/delete; retain source attribution and consent record for every reference. Prevent reference leakage across evaluation splits. Test repeated recordings from one device across changed rooms/chains, different devices with similar characteristics, codec/gain/sample-rate changes, missing features, and adversarial collisions. The UI must show “no reliable match” for unsupported distributions rather than force a nearest result.

## Sources

- [Forensic limitations: similarity and fingerprints](../research/forensic-limitations.md).
- [Validation corpus](validation-corpus.md) for leakage-resistant data splits and metrics.
- Current research does not establish that HumTrace's present feature set uniquely identifies recording equipment or location.
