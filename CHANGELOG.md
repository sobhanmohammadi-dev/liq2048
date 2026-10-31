# Changelog

All notable changes are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

## [1.0.0] - 2026-10-31

### Added
- Physics blob-merging gameplay (position-based dynamics) with chains, squash/pop feedback and particles.
- Five game modes: Classic, Time Attack, Target (escalating 256/512/1024), Daily Challenge (UTC-day seed) and Zen.
- Flat pixel-art HUD, skippable tutorial, pause menu, settings (language, volume, fullscreen, theme) and scene fades.
- Progression: per-mode best scores, nine achievements, non-punitive daily streak, four unlockable themes.
- English and Persian localization with right-to-left layout; string tables in `assets/lang/`.
- Built-in bitmap font; optional SDL_ttf fonts, SDL_mixer sound and music via drop-in assets.
- CMake build for Windows, Linux, macOS, Android and iOS, with presets.
- Unit test suite (`ctest`), headless `--screenshot` developer tool, GitHub Actions CI, CodeQL and release workflows.
- Project documentation under `docs/`.

[Unreleased]: https://github.com/Sobhanmohammadi/Liq2048/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/Sobhanmohammadi/Liq2048/releases/tag/v1.0.0
