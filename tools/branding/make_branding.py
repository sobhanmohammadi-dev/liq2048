#!/usr/bin/env python3
"""Generates the Liq2048 logo, README banner and social-preview image as pixel-art SVGs.

The pixel glyphs are read straight from src/gfx/Gfx.cpp so the branding uses exactly
the same font as the game. Everything is drawn from coloured rectangles, so there are
no font or image dependencies. Run from anywhere:

    python3 tools/branding/make_branding.py            # writes docs/branding/*.svg
    python3 tools/branding/make_branding.py --png      # also renders PNGs (needs rsvg-convert)
"""
import math
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "docs" / "branding"

# Default theme (src/core/Themes.hpp)
BG, PANEL, GOLD, DIM, TEXT = "#14122b", "#241f4a", "#ffc83d", "#7f7ab0", "#ffffff"
BLOB = ["#5ee6ff", "#3fa9ff", "#a89cff", "#e0a8ff", "#ff7ad9", "#ff5c7a",
        "#ffc83d", "#ffe66b", "#b8f26b", "#4fe0a0", "#f2f2f2", "#ffd700"]
DARK = "#1a1a2e"

GLYPHS = {m.group(1).upper(): m.group(2)
          for m in re.finditer(r"\{'(.)', \"([01]{35})\"\}", (ROOT / "src/gfx/Gfx.cpp").read_text())}


def shade(hex_color, t):
    """Mix a colour toward black (t>0) or white (t<0)."""
    c = [int(hex_color[i:i + 2], 16) for i in (1, 3, 5)]
    target = 0 if t > 0 else 255
    t = abs(t)
    return "#%02x%02x%02x" % tuple(round(v + (target - v) * t) for v in c)


class Canvas:
    """A grid of square 'pixels' emitted as merged horizontal runs of <rect>s."""

    def __init__(self, w, h, unit):
        self.w, self.h, self.u = w, h, unit
        self.items = []

    def rect(self, x, y, w, h, color):
        u = self.u
        self.items.append(f'<rect x="{x * u:g}" y="{y * u:g}" width="{w * u:g}" height="{h * u:g}" fill="{color}"/>')

    def disc(self, cx, cy, r, color):
        for dy in range(-int(r), int(r) + 1):
            half = math.sqrt(max(0.0, r * r - (dy + 0.0) ** 2))
            x0, x1 = round(cx - half), round(cx + half)
            if x1 > x0:
                self.rect(x0, round(cy) + dy, x1 - x0, 1, color)

    def blob(self, cx, cy, r, level, label=None):
        col = BLOB[level]
        self.disc(cx, cy, r + 1, shade(col, 0.6))
        self.disc(cx, cy, r, col)
        self.rect(round(cx - r * 0.55), round(cy - r * 0.55), max(2, round(r * 0.22)), 1, shade(col, -0.55))
        if label:
            sc = 2 if r >= 20 else 1
            self.text(label, cx, cy, sc, DARK if level in (1, 2, 3, 6, 7, 8, 9, 10, 11) else "#ffffff", center=True)

    def text_width(self, s, scale):
        return (len(s) * 6 - 1) * scale

    def text(self, s, x, y, scale, color, center=False):
        """Draw text; if center, (x, y) is the centre, else the top-left corner."""
        if center:
            x -= self.text_width(s, scale) / 2
            y -= 3.5 * scale
        x, y = round(x), round(y)
        for i, ch in enumerate(s.upper()):
            g = GLYPHS.get(ch)
            if not g:
                continue
            for row in range(7):
                col = 0
                while col < 5:
                    if g[row * 5 + col] == "1":
                        start = col
                        while col < 5 and g[row * 5 + col] == "1":
                            col += 1
                        self.rect(x + (i * 6 + start) * scale, y + row * scale, (col - start) * scale, scale, color)
                    else:
                        col += 1

    def stars(self, seed=7, n=60):
        s = seed
        for _ in range(n):
            s = (s * 1103515245 + 12345) & 0x7FFFFFFF
            x = s % self.w
            s = (s * 1103515245 + 12345) & 0x7FFFFFFF
            y = s % self.h
            self.rect(x, y, 1, 1, "#4a4580" if (x + y) % 3 else DIM)

    def svg(self, title, desc):
        u = self.u
        body = "\n  ".join(self.items)
        return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w * u}" height="{self.h * u}" '
                f'viewBox="0 0 {self.w * u} {self.h * u}" shape-rendering="crispEdges" role="img" '
                f'aria-labelledby="t d">\n  <title id="t">{title}</title>\n  <desc id="d">{desc}</desc>\n  {body}\n</svg>\n')


def logo():
    c = Canvas(128, 128, 4)
    c.rect(0, 0, 128, 128, BG)
    c.stars(3, 40)
    # Border with notched corners for a pixel "rounded" look.
    for rect in [(3, 0, 122, 3), (3, 125, 122, 3), (0, 3, 3, 122), (125, 3, 3, 122)]:
        c.rect(*rect, GOLD)
    for x, y in [(2, 2), (125, 2), (2, 125), (125, 125)]:
        c.rect(x, y, 1, 1, GOLD)
    c.blob(25, 26, 14, 0, "1")
    c.blob(103, 30, 11, 4, "8")
    c.blob(64, 70, 42, 11)
    c.text("2048", 64, 70, 3, DARK, center=True)
    return c.svg("Liq2048 logo", "A large gold blob labelled 2048 with two smaller blobs on a dark pixel-art tile.")


def banner(height_cells, with_tagline=True):
    c = Canvas(320, height_cells, 4)
    oy = (height_cells - 80) // 2
    c.rect(0, 0, 320, height_cells, BG)
    c.stars(11, 110 if height_cells > 80 else 70)
    # Frame lines like the in-game container.
    c.rect(0, 0, 320, 2, GOLD)
    c.rect(0, height_cells - 2, 320, 2, GOLD)
    # Wordmark with a hard drop shadow.
    word = "LIQ2048"
    x0 = (320 - c.text_width(word, 4)) // 2
    c.text(word, x0 + 2, oy + 22, 4, "#000000")
    c.text(word, x0, oy + 20, 4, GOLD)
    if with_tagline:
        tag = "MERGE THE LIQUID. REACH 2048."
        c.text(tag, (320 - c.text_width(tag, 1)) // 2, oy + 58, 1, TEXT)
        sub = "FREE - OPEN SOURCE - NO ADS"
        c.text(sub, (320 - c.text_width(sub, 1)) // 2, oy + 69, 1, DIM)
    # Blob clusters (sizes grow with value, like the game).
    left = [(14, 62, 6, 0, "1"), (31, 55, 9, 1, "2"), (52, 63, 12, 2, "4"), (22, 30, 8, 3, "8")]
    right = [(262, 64, 11, 4, "16"), (289, 56, 15, 7, "128"), (307, 24, 10, 5, "32"), (268, 26, 7, 1, "2")]
    for cx, cy, r, lv, lab in left + right:
        c.blob(cx, cy + oy, r, lv, lab)
    return c


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "logo.svg").write_text(logo())
    (OUT / "banner.svg").write_text(banner(80).svg(
        "Liq2048 banner", "Liq2048 pixel-art wordmark with numbered liquid blobs on a dark starry background."))
    (OUT / "social-preview.svg").write_text(banner(160).svg(
        "Liq2048 social preview", "Liq2048 pixel-art wordmark with numbered liquid blobs."))
    print("wrote SVGs to", OUT)
    if "--png" in sys.argv:
        tool = shutil.which("rsvg-convert")
        if not tool:
            sys.exit("rsvg-convert not found; install librsvg to render PNGs")
        for name, w in [("logo", 512), ("banner", 1280), ("social-preview", 1280)]:
            subprocess.run([tool, "-w", str(w), str(OUT / f"{name}.svg"), "-o", str(OUT / f"{name}.png")], check=True)
        print("wrote PNGs")


if __name__ == "__main__":
    main()
