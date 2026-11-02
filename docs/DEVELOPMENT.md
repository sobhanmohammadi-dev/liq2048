# Development guide

## Daily workflow

```bash
cmake -S . -B build -G Ninja -DLIQ_WARNINGS_AS_ERRORS=ON   # same flags as CI
cmake --build build
ctest --test-dir build --output-on-failure
./build/liq2048
```

`compile_commands.json` is generated in the build dir for clangd/IDE support.

## Tests

`tests/test_main.cpp` uses a tiny built-in runner (`TEST(name)` / `CHECK(expr)`), so there is nothing to install.
Add a test whenever you change `game/`, `save/`, `i18n/` or `core/` logic. Tests must not open a window.

## CI

| Workflow | Trigger | What it does |
|----------|---------|--------------|
| `ci.yml` | push to `main`, PRs | Linux (gcc + clang), macOS, Windows builds with warnings as errors, unit tests, headless render smoke test; Android build is experimental (`continue-on-error`) |
| `codeql.yml` | push, PR, weekly | CodeQL static analysis for C/C++ |
| `release.yml` | tag `v*.*.*` | Build + test + CPack ZIP/TGZ on 3 OSes, publish a GitHub release |
| `dependabot.yml` | weekly | Keeps GitHub Action versions current |

Recommended branch protection for `main`: require pull requests, require the Linux/macOS/Windows jobs to pass,
and require CODEOWNERS review.

## Releasing

1. Update `CHANGELOG.md` (move *Unreleased* to the new version with a date).
2. Bump the version in `CMakeLists.txt` (`project(... VERSION x.y.z)`); `LIQ_VERSION` follows automatically.
3. Commit, then tag and push:
   ```bash
   git tag -a vX.Y.Z -m "Liq2048 X.Y.Z"
   git push origin main vX.Y.Z
   ```
4. The Release workflow publishes archives; edit the release notes if needed.

Tag **every finished station** (each roadmap milestone, see [ROADMAP.md](ROADMAP.md)) with an annotated
`vX.Y.Z` tag on the commit that closes it, after CI is green. Pre-releases use `vX.Y.Z-beta.N`. Never move or
reuse a tag.

## Regenerating screenshots and branding

```bash
# README screenshots (headless, deterministic)
./build/liq2048 --screenshot /tmp/s.bmp --scene game --frames 700
python3 -c "from PIL import Image; Image.open('/tmp/s.bmp').convert('RGB').save('docs/images/game.png')"

# Logo, banner, social preview (SVG + PNG)
python3 tools/branding/make_branding.py --png     # PNG needs rsvg-convert (librsvg)
```

Upload `docs/branding/social-preview.png` in *Settings → General → Social preview* on GitHub.

## Suggested repository settings

- About: *Physics-based liquid blob merging puzzle in C++17 and SDL2*
- Topics: `game`, `cpp`, `cpp17`, `sdl2`, `cmake`, `puzzle`, `physics`, `2048`, `suika`, `pixel-art`, `android`, `ios`, `persian`, `rtl`, `open-source-game`
- Enable Discussions, Dependabot alerts and private vulnerability reporting.

## Roadmap

See [ROADMAP.md](ROADMAP.md).
