# Contributing to Liq2048

Thanks for your interest! Bug reports, translations, new themes, modes and fixes are all welcome.

## Ground rules

- Be kind: see the [Code of Conduct](CODE_OF_CONDUCT.md).
- **No monetization, ads, tracking or manipulative retention mechanics.** Liq2048 is free and respects players.
- Open an issue before large changes so we can agree on the approach.
- One logical change per pull request.

## Getting set up

See [docs/BUILDING.md](docs/BUILDING.md). The quick version on Linux:

```bash
sudo apt install cmake ninja-build libsdl2-dev libsdl2-ttf-dev libsdl2-mixer-dev
cmake -S . -B build -G Ninja -DLIQ_WARNINGS_AS_ERRORS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Read [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) first to see where things live.

## Workflow

1. Fork and create a branch: `feat/short-name`, `fix/short-name`, `docs/short-name`.
2. Make your change, **add or update tests** for logic in `src/game`, `src/save`, `src/i18n`, `src/core`.
3. Run the build with warnings as errors and `ctest`.
4. For UI changes include a screenshot: `./build/liq2048 --screenshot shot.bmp --scene game --frames 600`.
5. Update `CHANGELOG.md` under **Unreleased**.
6. Open a pull request using the template. CI must pass on Linux, macOS and Windows.

Commit messages follow [Conventional Commits](https://www.conventionalcommits.org/):
`feat: add endless-with-obstacles mode`, `fix: blob sticks to wall`, `docs: clarify Android build`.

## Code style

- C++17, RAII, no raw owning pointers, no mutable global state (pass `Ctx&`).
- Tunables live in `src/core/Config.hpp`; do not scatter magic numbers.
- Comment the *why* of non-obvious systems (physics, layout math).
- 4-space indent, 120 columns. A `.clang-format` and `.editorconfig` are provided; run
  `clang-format -i` on files you touch if you have it.
- Every user-visible string goes through `Loc::tr`.

## Translations

Copy `assets/lang/en.json` to `assets/lang/<code>.json`, translate the values, and follow
[docs/LOCALIZATION.md](docs/LOCALIZATION.md). The test suite verifies that every English key exists
in every shipped language. Scripts that need a new font should say which font you tested with.

## Reporting security issues

Do not open a public issue. See [SECURITY.md](SECURITY.md).

## License

By contributing you agree that your contributions are licensed under the [MIT License](LICENSE).
