#!/usr/bin/env bash
# Usage: scripts/package_linux.sh <cmake-build-dir> <out-dir>
# Stages the CLI + GUI (dynamic Qt6; Qt is NOT bundled, see THIRD-PARTY-NOTICES),
# builds a tarball and its SHA-256, and records the source commit.
set -euo pipefail
build=${1:?build dir}; out=${2:?out dir}
version=$(grep -oP 'kVersion = "\K[^"]+' include/humtrace/version.hpp)
name="humtrace-${version}-linux-x86_64"
stage="$out/$name"
rm -rf "$stage"; mkdir -p "$stage/bin" "$stage/share/applications" "$stage/share/icons"
install -m755 "$build/humtrace" "$build/humtrace-gui" "$stage/bin/"
strip "$stage/bin/humtrace" "$stage/bin/humtrace-gui"
install -m644 packaging/humtrace.desktop "$stage/share/applications/"
install -m644 packaging/humtrace.svg "$stage/share/icons/"
install -m644 README.md THIRD-PARTY-NOTICES docs/user-guide/limitations.md schema/analysis-report-v1.schema.json "$stage/"
printf 'version=%s\ncommit=%s\nRuntime: Qt 6 Widgets (apt: libqt6widgets6 libqt6gui6 libqt6core6) for humtrace-gui; the CLI has no Qt dependency.\n' \
  "$version" "$(git rev-parse HEAD)" > "$stage/BUILD-INFO.txt"
tar -C "$out" --sort=name --owner=0 --group=0 -czf "$out/$name.tar.gz" "$name"
(cd "$out" && sha256sum "$name.tar.gz" > "$name.tar.gz.sha256")
cat "$out/$name.tar.gz.sha256"
