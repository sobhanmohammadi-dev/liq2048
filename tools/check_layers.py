#!/usr/bin/env python3
"""Fails when a lower layer includes a higher one (see docs/ARCHITECTURE.md, "Layers").

Usage: python3 tools/check_layers.py   (run from the repository root; also run by CI and by `ctest`-less release checks)
"""
import re, sys, pathlib

# module -> modules it may include (besides itself). Lower layers first.
ALLOWED = {
    "core":   {"gfx", "i18n", "save", "audio", "ui", "scenes", "game", "battle", "net"},  # App wires everything together
    "net":    {"core"},
    "i18n":   {"core"},
    "gfx":    {"core", "i18n"},
    "audio":  {"core"},
    "game":   {"core", "gfx", "i18n", "save"},  # Mode rules read stats and strings,
    "save":   {"core", "game"},
    "battle": {"core", "game", "net", "save"},
    "ui":     {"core", "gfx", "i18n", "save", "audio", "scenes", "game"},
    "scenes": {"core", "gfx", "i18n", "save", "audio", "ui", "game", "battle", "net"},
}
bad = []
for f in sorted(pathlib.Path("src").rglob("*")):
    if f.suffix not in (".cpp", ".hpp") or len(f.parts) < 3:
        continue
    mod = f.parts[1]
    for n, line in enumerate(f.read_text(errors="ignore").splitlines(), 1):
        m = re.match(r'\s*#include\s+"([a-z0-9]+)/', line)
        if m and m.group(1) in ALLOWED and m.group(1) != mod and m.group(1) not in ALLOWED.get(mod, set()):
            bad.append(f"{f}:{n}: {mod} must not include {m.group(1)}")
print("\n".join(bad) or "layers ok")
sys.exit(1 if bad else 0)
