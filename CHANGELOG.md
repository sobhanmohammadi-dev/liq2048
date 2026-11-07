# Changelog

All notable changes are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

See [docs/ROADMAP.md](docs/ROADMAP.md) for what is planned next.

## [1.7.4]

### Fixed
- **Watching a run code:** you can now pause (play/pause button, 1x/2x/4x speed and a progress bar), and a replay always plays to the end of the recorded run instead of stopping early. Replays were drifting from the original because the performance mode, the Zen drain and the Rising Tide line depended on frame time; they are now tied to physics steps, and the run code records the end step and the performance-mode switches.
- **Rising Tide:** the line stopped moving after about 2:24, when it had sunk its full 230 px. It now keeps moving as a swell (up and down by 22 px every 12 s).
- Hint box and undo button no longer show while watching a replay.

### Changed
- **Liquid physics:** much closer to water. Viscosity 300 to 60, almost no artificial damping, walls are nearly free-slip, no surface tension for a drop at rest (a settled pool is flat, with travelling waves), higher terminal speed (splashes), velocity spikes capped. Merge reach was widened so merge rates stay the same.
- Run codes are now `LQ2-` (older `LQ1-` codes still load, with a warning that the liquid may differ because of the new physics).

### Added
- **Copy run code** in the pause menu: the run from the first drop to the pause.
- Run codes for puzzles (result screen: Copy code and Watch replay).
- `docs/GDD.md`, a complete game design document.

## [1.7.3]

### Added
- **Custom theme** (Settings, below the theme selector): an editor for the name, all 34 colours and the sound of every slot (3 music loops and 9 effect groups). A sound can come from any built-in palette or from your own WAV/OGG file. The English name is required, names in other languages are optional and English is used where a language has none.
- **Export theme / Import theme** in Additional features. The `.liqtheme` file holds the names, colours, music and sound effects. Import also reads a file dropped on the window (desktop), the themes folder, or text from the clipboard (themes without audio files, handy on phones).
- Each built-in theme (Default, Steel, Sunset, Forest) now has its own music and sound effects.

### Changed
- Sounds are now in `assets/sounds/<palette>/` and `assets/music/<palette>/` as optimized OGG; the full-quality 24-bit FLAC masters are in `original_assets/audio/`.
- The pixel fonts are no longer embedded in the code: they are the BDF files `assets/fonts/pixel-5x7.bdf` and `pixel-3x5.bdf` (also in `original_assets/fonts/`).
- Licences: code MIT; sounds, music and the Liq Pixel fonts CC BY 4.0; Vazirmatn keeps its own SIL OFL 1.1.
- Additional features has two more rows, so the rows are more compact.

## [1.7.2]

### Added
- **Pass and play** (Battle menu): two players share one Classic pot and alternate drops; merges score for whoever
  dropped last; the pot overflowing ends the match; result screen with rematch.
- **Ghost pace:** in Classic, Time Attack, Rain, Tide and Zen the HUD shows how far ahead or behind you are of your best run.
- **History** tab on Progress: bar charts of your last 12 scores per mode.
- **Save result image** button on the game-over screen (PNG of the final pot, saved next to the save file).
- Pause menu lists the desktop keys; release builds publish a `SHA256SUMS` file.

## [1.7.1]

### Added
- **Additional features** screen (main menu, second button): watch a run code, **player card** (your stats as a PNG,
  with the text also copied), player name (used in Battle), **back up save** (clipboard + file) and **restore save**
  (from the clipboard; the previous save is kept as `save-before-restore.txt`), authors, quit.
- New **mode board**: a scrollable page following the design sketch (wide Battle tile, tall Puzzle tile beside
  Classic and Zen, then Time Attack, Target, Daily, Rain, Tide and Weekly), each tile with its own live animation
  (two drops bumping for Battle, three drops drifting for Puzzle, one calm drop and ripples for Zen, a clock for
  Time Attack, and so on). Drag, mouse wheel, arrow keys and gamepad all scroll or select.
- Text input support (used for the player name).

### Changed
- The main menu is the classic one again, now with Start, Additional features, Progress and Settings.

### Fixed (mobile feel)
- A tap that lands while the 0.4 s drop cooldown is still running is now kept and fires when the cooldown ends,
  instead of being lost (this felt like having to tap twice).
- Buttons accept a release a few pixels off the button the touch began on, so a slightly rolling finger still
  counts as a tap.
- Performance governor: if the frame time stays above about 24 ms, the liquid drops to one substep (Daily, Puzzle
  and Battle always keep the reference physics), and the simulation never tries to catch up more than 3 steps.
  This is untested on a real phone.

## [1.7.0]

### Added
- **Battle mode.** Two players get the identical drop queue and play in their own pots for a fixed time (60, 90, 120
  or 180 s); the higher score wins, a tie goes to the higher droplet level. Modifiers (low gravity, heavy, thick,
  bonus drops) apply to both pots.
- **Versus Bot** with four difficulties: Easy, Medium, Hard and Adaptive (adjusts to how well you play).
- **Versus Human over the local network (LAN):** *Host a game* opens a waiting screen; on another device the game
  appears in *Browse games*; the guest taps it and the host is told a player joined and presses *Start*.
- **View Opponent** button: shows the opponent's pot live (about 10 updates a second, smoothed). It is a toggle; the
  opponent is never shown permanently next to your pot, on phones or on desktop.
- Result screen with both scores, win/loss/draw record (also on the Progress page) and **Rematch**.
- New main menu: Classic, Battle, Puzzle and Zen are the four big tiles; everything else is under *More modes*.
- 11 more puzzles (26 in total), solved and verified by the generator.
- Battle translations in all 8 languages.

### Changed
- Android declares the network permissions Battle needs; iOS declares a local-network usage description. The game
  still never contacts the internet: networking starts only when the player opens Battle's LAN screens.

## [1.6.1]

### Added
- Authors page (menu, bottom left): who made the game, thanks to SDL2, Vazirmatn and Claude (sound and music).
- Vazirmatn font bundled in the repository (`assets/fonts/fa.ttf`, SIL OFL).

### Fixed
- Persian and Arabic text: letters were disconnected and in the wrong order on phones. The game now shapes
  Arabic-script text itself (joined letters, ligatures, right-to-left order), identical on every platform.
- Left-hand mode: the labels on the settings page were hidden behind the buttons.
- Phones with tall screens: the game now fills the whole display (the bars around it use the theme gradient).
- The numbers inside droplets no longer jitter while the liquid rests.

### Changed
- Harder: bigger drops appear as a run goes on, and the overflow line sits lower in every mode except Zen, Puzzle and Rising Tide.
- Rising Tide: the line sinks all the time (up to 230 px), merges lift it but never above its start, and it moves smoothly.
- The language button in the top corner was removed; change the language in Settings.
- Roadmap: v1.7.0 is now "Battle" (PvP over LAN, PvE, more puzzles); store release plans were removed.

## [1.6.0]

### Added
- Gamepad support, mouse-wheel aiming, F3 frame-time overlay.
- Fuzz smoke tests and an ASan + UBSan CI job.

## [1.5.0]

### Added
- Run codes: copy a run as a short checksummed `LQ1-…` code and watch a friend's code from the menu.
- "Copy result" text for sharing a score.
- Drop positions are quantised to 1/128 px (needed for lossless run codes; no visible change).

## [1.4.1]

### Added
- Replay recorder and a "Watch replay" button on the game-over screen (8 languages). Watching never changes stats, bests or achievements.
- `Replay` text format (`LIQREPLAY 1`) and a regression test for playback determinism.

## [1.4.0]

### Added
- "Liquid quality" setting (Low / Medium / High).
- Determinism test for the physics at all quality levels.

### Changed
- Faster physics step (about 8% in our benchmark).
- Pot shapes, replays, new themes and foam effects were moved to later milestones (see docs/ROADMAP.md).

## [1.3.0] - 2026-11-03

Station "Modes".

### Added
- **Endless Rain** (drops fall by themselves, faster over time), **Rising Tide** (the overflow line sinks, merges
  push it back), **Weekly Challenge** (one seed per week plus a special rule, own weekly best) and **Puzzle**
  (15 verified levels with a pre-filled pot, fixed drop queue, goal tile and 1 to 3 stars).
- Modifiers: low gravity, heavy gravity, thick liquid, bonus drops (unranked runs).
- Mode screen with two pages; level picker for puzzles.
- Progress screen with tabs: scores with bronze/silver/gold medals, 19 achievements (10 new), lifetime statistics.
- First-run coach hints inside the real pot, and a one-time hint the first time each mode is played.
- `tools/puzzlegen` (generates and verifies the puzzle pack), `docs/PUZZLES.md`, tests for the new rules.

### Changed
- Save format extended (stats, modifier, weekly best, puzzle stars); old saves load unchanged.

## [1.2.0] - 2026-11-03

Station "Reach".

### Added
- Six new languages: Spanish, French, German, Turkish, Portuguese and Arabic (8 in total), with a language picker in
  Settings. The pixel font folds accents to base letters; Arabic uses the same RTL path as Persian.
- Colour-vision modes (Red/Green and Blue/Yellow safe droplet palettes, optimised for distinguishability under
  colour-blindness simulation) with a live preview, and a High contrast option.
- Reduced motion option (instant scene changes, no particles, rings, bubbles, shake or drifting backdrop).
- Left-hand HUD layout and a Big touch areas option; every button also gets an invisible touch margin on phones.
- Settings now have three pages (General, Game feel, Access).
- `docs/ACCESSIBILITY.md`; tests that every language is complete, that accents fold correctly and that the
  accessibility palettes stay distinct.
- Dev tool flags `--page` and `--set` for screenshots.

### Changed
- The in-game language button toggles between English and the last other language used.

## [1.1.0] - 2026-11-02

Station "Feel".

### Added
- Undo in Classic and Zen: one step back per drop; start with 1 charge, earn +1 per chain of 3 or more (max 3).
  Button on the HUD, keys `U` or `Z`.
- Haptic feedback on phones for drops, merges (stronger with level and chain), heavy impacts and game over.
- Settings page "Game feel": aim guide, vibration (phones), screen shake, slow motion, Calm mode.
- Calm mode: no screen shake, slow motion, blinking or flashes, and fewer particles.
- Slow motion on very large merges and long chains.
- Auto-pause when the window loses focus or the app goes to the background.
- Window and taskbar icon on desktop.
- Larger accent-coloured pop-ups for chains of 3 or more.

### Changed
- Settings are split over two pages (General, Game feel); the Android app now declares the vibration permission.

## [1.0.0] - 2026-11-02

First stable release. Every earlier development station is tagged in git (`v0.1.0` to `v0.5.0`, listed below).

### Gameplay
- Five modes: Classic, Time Attack, Target (escalating 256/512/1024), Daily Challenge (UTC-day seed) and Zen.
- Real liquid: a particle fluid simulation. Drops fall as droplets, splash on impact, spread and level out in
  the pot; same-level drops merge, chain, and blend colours where liquids touch.
- Progression: per-mode best scores, nine achievements, non-punitive daily streak, four unlockable themes.
- Skippable tutorial, pause menu, settings (language, music, sound, frame rate, FPS counter, fullscreen, theme).

### Look and sound
- Pixel-art style on a fixed 270x480 grid with integer scaling: dithered fused droplets, 1px outlines, flat
  numerals, built-in pixel font family, icon set, blinds transitions. No glow, no lighting.
- Audio shipped as OGG files in `assets/sounds` and `assets/music`: merge pitch follows the droplet level,
  splashes follow impact strength, and calm/intense music loops cross-fade as the pot fills. Generated by
  `tools/audiogen` (dev tool, `LIQ_BUILD_TOOLS`).
- English and Persian with right-to-left layout; string tables in `assets/lang/`.

### Platforms and project
- Windows, Linux, macOS, Android (APK + AAB) and iOS (unsigned IPA) via CMake, the `android/` Gradle project
  and GitHub Actions (CI, CodeQL, release workflow).
- Unit tests (`ctest`), headless `--screenshot` tool, documentation under `docs/`.

## Development stations (pre-release tags)

| Tag | Station |
|-----|---------|
| v0.1.0 | Playable prototype: rigid blob physics, five modes, localization, CI |
| v0.2.0 | Pixel-art visual overhaul, frame-rate setting |
| v0.3.0 | Liquid particle simulation |
| v0.4.0 | Sound engine and settling/splash physics |
| v0.5.0 | Android and iOS builds, self-contained desktop packages |

[Unreleased]: https://github.com/sobhanmohammadi-dev/Liq2048/compare/v1.3.0...HEAD
[1.3.0]: https://github.com/sobhanmohammadi-dev/Liq2048/compare/v1.2.0...v1.3.0
[1.2.0]: https://github.com/sobhanmohammadi-dev/Liq2048/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/sobhanmohammadi-dev/Liq2048/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/sobhanmohammadi-dev/Liq2048/releases/tag/v1.0.0
