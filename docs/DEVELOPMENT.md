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
   git tag -a v1.1.0 -m "Liq2048 1.1.0"
   git push origin main v1.1.0
   ```
4. The Release workflow publishes archives; edit the release notes if needed.

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

## Roadmap ideas

- Window/app icon from the logo; in-game language list for more languages
- On-screen replay of the best run; share-card export of the daily result
- Gamepad support
- Audio pack (CC0) and Persian font bundled with license files
- Android/iOS CI packaging
