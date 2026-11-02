#!/usr/bin/env python3
"""Regenerates the game's audio: synthesizes every sound with liq_audiogen and encodes it to OGG Vorbis.

    cmake -S . -B build -DLIQ_BUILD_TOOLS=ON && cmake --build build --target liq_audiogen
    python3 tools/audiogen/build_audio.py [--bin build/liq_audiogen]

Needs ffmpeg with libvorbis. Output: assets/sounds/*.ogg and assets/music/*.ogg (committed to the repo).
"""
import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=str(ROOT / "build" / "liq_audiogen"))
    a = ap.parse_args()
    if not shutil.which("ffmpeg"):
        print("ffmpeg not found", file=sys.stderr)
        return 1
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run([a.bin, tmp], check=True)
        for kind, quality in (("sounds", "5"), ("music", "5")):
            out = ROOT / "assets" / kind
            out.mkdir(parents=True, exist_ok=True)
            for old in out.glob("*.ogg"):
                old.unlink()
            for wav in sorted((Path(tmp) / kind).glob("*.wav")):
                dst = out / (wav.stem + ".ogg")
                subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(wav), "-c:a", "libvorbis", "-q:a", quality,
                                "-map_metadata", "-1", "-fflags", "+bitexact", "-flags:a", "+bitexact", str(dst)], check=True)
                print("wrote", dst.relative_to(ROOT), f"{dst.stat().st_size // 1024} KB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
