#!/usr/bin/env python3
"""Regenerates the game's audio for the four sound palettes (default, steel, sunset, jungle).

    cmake -S . -B build -DLIQ_BUILD_TOOLS=ON && cmake --build build --target liq_audiogen
    python3 tools/audiogen/build_audio.py [--bin build/liq_audiogen]

Needs ffmpeg with libvorbis and flac. Output, per palette:
  original_assets/audio/<palette>/{sounds,music}/*.flac   24-bit lossless masters (CC BY 4.0)
  assets/sounds/<palette>/*.ogg, assets/music/<palette>/*.ogg   optimized OGG Vorbis used by the game (CC BY 4.0)
"""
import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PALETTES = ("default", "steel", "sunset", "jungle")
QUALITY = {"sounds": "3", "music": "4"}
COMMON = ["-map_metadata", "-1", "-fflags", "+bitexact", "-flags:a", "+bitexact"]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=str(ROOT / "build" / "liq_audiogen"))
    a = ap.parse_args()
    if not shutil.which("ffmpeg"):
        print("ffmpeg not found", file=sys.stderr)
        return 1
    for kind in QUALITY:  # drop the old flat layout
        for old in (ROOT / "assets" / kind).glob("*.ogg"):
            old.unlink()
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run([a.bin, tmp], check=True)
        for pal in PALETTES:
            for kind, q in QUALITY.items():
                ogg = ROOT / "assets" / kind / pal
                flac = ROOT / "original_assets" / "audio" / pal / kind
                for d in (ogg, flac):
                    d.mkdir(parents=True, exist_ok=True)
                    for old in d.glob("*"):
                        if old.suffix in (".ogg", ".flac"):
                            old.unlink()
                for wav in sorted((Path(tmp) / pal / kind).glob("*.wav")):
                    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(wav), "-c:a", "flac", "-sample_fmt", "s32",
                                    "-bits_per_raw_sample", "24", *COMMON, str(flac / (wav.stem + ".flac"))], check=True)
                    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(wav), "-ar", "44100", "-ac", "2", "-c:a",
                                    "libvorbis", "-q:a", q, *COMMON, str(ogg / (wav.stem + ".ogg"))], check=True)
                print(pal, kind, "ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
