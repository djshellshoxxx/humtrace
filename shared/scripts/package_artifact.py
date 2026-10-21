#!/usr/bin/env python3
"""Create a deterministic release archive (tar.gz, or zip with --zip).

Usage: package_artifact.py DIR OUT [--zip]
Entries are sorted, with fixed owner, mode and mtime taken from
SOURCE_DATE_EPOCH, so identical inputs yield byte-identical archives on
every platform (bsdtar and GNU tar differ, hence no external tar).
"""
import gzip
import io
import os
import sys
import tarfile
import zipfile
from pathlib import Path


def entries(root: Path):
    yield root
    for path in sorted(root.rglob("*"), key=lambda p: p.as_posix()):
        yield path


def main() -> int:
    src, out = Path(sys.argv[1]), Path(sys.argv[2])
    use_zip = "--zip" in sys.argv[3:]
    epoch = int(os.environ.get("SOURCE_DATE_EPOCH", "0"))
    base = src.parent
    if use_zip:
        # ZIP cannot represent dates before 1980.
        stamp = time_tuple(max(epoch, 315532800))
        with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as zf:
            for p in entries(src):
                if p.is_dir():
                    continue
                info = zipfile.ZipInfo(p.relative_to(base).as_posix(), stamp)
                info.external_attr = (0o755 if p.suffix in ("", ".exe") else 0o644) << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                zf.writestr(info, p.read_bytes())
    else:
        raw = io.BytesIO()
        with tarfile.open(fileobj=raw, mode="w", format=tarfile.PAX_FORMAT) as tf:
            for p in entries(src):
                info = tf.gettarinfo(str(p), p.relative_to(base).as_posix())
                info.uid = info.gid = 0
                info.uname = info.gname = ""
                info.mtime = epoch
                info.mode = 0o755 if p.is_dir() or os.access(p, os.X_OK) else 0o644
                if p.is_dir():
                    tf.addfile(info)
                else:
                    with p.open("rb") as fh:
                        tf.addfile(info, fh)
        with open(out, "wb") as fh, gzip.GzipFile("", "wb", 9, fh, mtime=0) as gz:
            gz.write(raw.getvalue())
    return 0


def time_tuple(epoch: int):
    import time
    return time.gmtime(epoch)[:6]


if __name__ == "__main__":
    sys.exit(main())
