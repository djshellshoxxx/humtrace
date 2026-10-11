#!/usr/bin/env python3
"""Write small valid WAV/RF64/BW64 seed files for fuzz_wav into a directory.

Usage: make_seeds.py <output-dir>
Each seed is prefixed with one control byte consumed by fuzz_wav.cpp.
"""
import pathlib
import struct
import sys

EXT_SUFFIX = bytes([0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71])


def chunk(tag: bytes, payload: bytes, size: int | None = None) -> bytes:
    pad = b"\0" if len(payload) % 2 else b""
    return tag + struct.pack("<I", len(payload) if size is None else size) + payload + pad


def fmt(code: int, channels: int, bits: int) -> bytes:
    align = channels * bits // 8
    return chunk(b"fmt ", struct.pack("<HHIIHH", code, channels, 48000, 48000 * align, align, bits))


def riff(body: bytes) -> bytes:
    return b"RIFF" + struct.pack("<I", len(body) + 4) + b"WAVE" + body


def large(container: bytes, fmt_chunk: bytes, samples: bytes) -> bytes:
    lst = b"LIST" + struct.pack("<I", 0xFFFFFFFF) + b"abcd"
    total = 12 + 8 + 28 + 12 + len(fmt_chunk) + len(lst) + 8 + len(samples)
    ds64 = struct.pack("<QQQI", total - 8, len(samples), len(samples) // 4, 1) + b"LIST" + struct.pack("<Q", 4)
    return (container + struct.pack("<I", 0xFFFFFFFF) + b"WAVE" + chunk(b"ds64", ds64) + fmt_chunk + lst +
            b"data" + struct.pack("<I", 0xFFFFFFFF) + samples)


def main() -> None:
    out = pathlib.Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    pattern = bytes((i * 37 + 11) & 0x7F for i in range(48))
    ext = struct.pack("<HHIIHHHHI", 0xFFFE, 2, 48000, 48000 * 6, 6, 24, 22, 20, 3) + struct.pack("<H", 1) + EXT_SUFFIX
    seeds = {
        "pcm16": riff(fmt(1, 1, 16) + chunk(b"data", pattern[:40])),
        "pcm24": riff(fmt(1, 2, 24) + chunk(b"LIST", b"xyz") + chunk(b"data", pattern[:30])),
        "pcm8": riff(fmt(1, 1, 8) + chunk(b"data", pattern[:9])),
        "float32": riff(fmt(3, 1, 32) + chunk(b"data", struct.pack("<3f", 0.25, -0.25, 0.0))),
        "float64": riff(fmt(3, 2, 64) + chunk(b"data", struct.pack("<2d", 0.5, -0.5))),
        "multi": riff(fmt(1, 1, 16) + chunk(b"data", pattern[:10]) + chunk(b"data", pattern[:6])),
        "extensible": riff(chunk(b"fmt ", ext) + chunk(b"data", pattern[:36])),
        "rf64": large(b"RF64", fmt(1, 2, 16), pattern[:32]),
        "bw64": large(b"BW64", fmt(3, 1, 32), struct.pack("<4f", 0.1, 0.2, 0.3, 0.4)),
    }
    for index, (name, data) in enumerate(seeds.items()):
        (out / f"seed_{name}.wav").write_bytes(bytes([index * 29 & 0xFF]) + data)


if __name__ == "__main__":
    main()
