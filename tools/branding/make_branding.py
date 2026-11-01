#!/usr/bin/env python3
"""Renders the Liq2048 logo, README banner and social preview with the game's own renderer.

The game binary draws the images (`--screenshot ... --scene logo|banner|social`), so the
branding always matches the in-game pixel art. Each image is cropped from the top-left of
the 270x480 logical screen and written as a crisp PNG plus a pixel-rect SVG.

    cmake --build build && python3 tools/branding/make_branding.py [--bin build/liq2048]

Needs Pillow (`pip install pillow`).
"""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "docs" / "branding"
SCALE = 2  # the game renders screenshots at 2x logical size
SHOTS = {"logo": (128, 128, 4), "banner": (270, 90, 4), "social": (270, 135, 5)}  # w, h, export zoom


def svg(img: Image.Image, zoom: int) -> str:
    w, h = img.size
    rects = []
    px = img.load()
    for y in range(h):
        x = 0
        while x < w:
            c = px[x, y]
            x2 = x + 1
            while x2 < w and px[x2, y] == c:
                x2 += 1
            rects.append(f'<rect x="{x}" y="{y}" width="{x2 - x}" height="1" fill="#{c[0]:02x}{c[1]:02x}{c[2]:02x}"/>')
            x = x2
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w * zoom}" height="{h * zoom}" '
            f'shape-rendering="crispEdges">\n' + "\n".join(rects) + "\n</svg>\n")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=str(ROOT / "build" / "liq2048"))
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        for name, (w, h, zoom) in SHOTS.items():
            bmp = Path(tmp) / f"{name}.bmp"
            subprocess.run([a.bin, "--screenshot", str(bmp), "--scene", name, "--frames", "2", "--lang", "en"], check=True,
                           cwd=ROOT)
            full = Image.open(bmp).convert("RGB")
            img = full.resize((full.width // SCALE, full.height // SCALE), Image.NEAREST).crop((0, 0, w, h))
            fname = {"social": "social-preview"}.get(name, name)
            img.resize((w * zoom, h * zoom), Image.NEAREST).save(OUT / f"{fname}.png")
            (OUT / f"{fname}.svg").write_text(svg(img, zoom))
            print("wrote", OUT / f"{fname}.png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
