# Native packaging and release specification

## Release targets

Initial supported targets: 64-bit Windows and one named Linux distribution baseline. Build separately on each native runner. Do not call a target supported solely because its toolkit claims platform support. macOS is outside v0.1 unless a native runner, signing/notarization plan, and release maintainer are available.

## Build outputs

- Windows: signed or clearly unsigned installer/portable archive, executable, required runtime libraries, license notices, version metadata, uninstall behavior, and clean-machine smoke test.
- Linux: documented AppImage or distro package selected after toolkit/dependency review; include application desktop entry, icon, runtime dependency policy, license notices, and clean VM smoke test.
- CLI remains independently buildable and usable without the GUI toolkit.
- Attach SBOM/dependency inventory, SHA-256 artifact checksums, source tag/commit, build logs, release notes, known limitations, and schema version.

## Reproducibility and supply chain

Pin compiler/toolchain and dependencies; build from a clean checkout; record exact commands and dependency hashes. Generate SBOM and license report. Do not bundle optional codecs or telemetry without explicit review. Build artifacts must not include analysis audio, local paths, credentials, or debug logs.

## Smoke and upgrade tests

On clean Windows/Linux VMs: install, launch, load valid mono/stereo WAV, cancel long analysis, export and reopen report, test unsupported/corrupt file, confirm source hash unchanged, uninstall, and verify no orphan user data except documented preference/session data. Test upgrade preserving user settings and reports. Test high-DPI scaling, keyboard-only workflow and screen reader basics on each platform.

## Release gate

All beta checklist items are green; native CI + ASan/UBSan where supported; independent corpus gate passed; dependency/license review complete; report schema frozen; privacy/limitations text reviewed; artifact checksums verified. Any open critical issue blocks beta. Do not publish “forensic-grade,” source-attribution, or authenticity claims.
