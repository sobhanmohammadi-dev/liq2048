# Changelog

All notable changes are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

## [1.1.0] - 2026-11-02

### Changed
- Complete visual overhaul to a polished pixel-art style on a fixed 270x480 grid with integer scaling.
- Droplets are now real liquid: a particle fluid simulation. Drops fall as droplets, splash on impact, spread
  and level out in the pot, and same-level drops merge. Colours blend where liquids touch using an ordered
  dither. No glow, no lighting, flat fills with a 1px outline and flat numerals.
- Droplets are larger (base radius 17) and the fall is gentler; render interpolation keeps motion smooth at
  any frame rate.
- New built-in pixel font family (5x7 text, 3x5 digits), icon set, bevelled UI, dithered backdrop and
  blinds scene transitions. Persian text keeps RTL layout.
- Pastel droplet palettes for all four themes.

### Added
- Procedural sound: every effect and a generative ambient soundtrack are synthesized in code (no audio files, no
  SDL_mixer). Merge pitch follows the droplet level, splashes follow impact strength, and the music gets busier
  as the pot fills. Separate Music and Sound volume settings; `--render-audio` dev tool.
- Android (APK + AAB) and iOS (unsigned IPA) builds in the release workflow; Android Gradle project under
  `android/`, iOS bundle configuration in CMake. macOS and Linux packages are now self-contained.
- Frame rate setting (VSync, 30, 60, 90, 120, 144, unlimited) and an optional FPS counter.
- Raster and FPS unit tests.

### Fixed
- Repository links now point to `sobhanmohammadi-dev`.

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

[Unreleased]: https://github.com/sobhanmohammadi-dev/Liq2048/compare/v1.1.0...HEAD
[1.1.0]: https://github.com/sobhanmohammadi-dev/Liq2048/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/sobhanmohammadi-dev/Liq2048/releases/tag/v1.0.0
