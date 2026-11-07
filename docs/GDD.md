# Liq2048: Game Design Document

| | |
|---|---|
| **Title** | Liq2048 |
| **Tagline** | Merge the liquid, reach 2048 |
| **Genre** | Physics merge puzzle ("Suika-style") with 2048 numbers |
| **Version of this document** | 1.7.4 (written after the fact, 2026-11-07, from the finished game and its history) |
| **Author / owner** | Sobhan Mohammadi |
| **Technology** | C++17, SDL2, SDL2_ttf, CMake |
| **Platforms** | Windows, Linux, macOS, Android, iOS (iOS and macOS built from the same source) |
| **Licence** | Code MIT; sounds, music and the Liq Pixel fonts CC BY 4.0; Vazirmatn font SIL OFL 1.1 |
| **Business model** | None. Free, no ads, no purchases, no tracking, no internet |

---

## 1. Overview

### 1.1 Concept

Liq2048 is a one-handed, short-session puzzle game. The player holds a numbered droplet above a tank, chooses where
to let it fall, and lets it go. Two droplets with the same number that touch merge into the next power of two
(1+1 = 2, 2+2 = 4 ... 1024+1024 = 2048). Droplets are not rigid balls: each one is a small cluster of fluid
particles, so they splash, spread, wobble and flow around each other like water. The player loses when liquid rests
above the overflow line for too long.

### 1.2 Pillars

1. **It should feel like liquid.** Falling, splashing, settling and merging are the reward. Physics is the game.
2. **Easy to learn, hard to master.** One gesture (drag, release). Depth comes from chains, planning the next
   droplet and managing the pile.
3. **Respectful.** No dark patterns. Progress is earned by playing; nothing is sold, nothing expires, nothing
   tracks the player.
4. **Everyone can play.** Eight languages (two right-to-left), colour-vision palettes, high contrast, reduced
   motion, one-handed layout, keyboard, mouse and touch.
5. **Small and self-contained.** Runs offline on modest phones; the executable draws everything in code and loads
   only a few external files (sounds, fonts, language files).

### 1.3 Target audience

- Casual players, ages 10+, who like 2048, Suika Game, Threes and Tetris-like relaxation.
- Commuters and short-break players on phones (sessions of 2-10 minutes).
- Players who care about privacy and want a game with no ads or accounts.
- Persian, Arabic, Turkish, Spanish, French, German and Portuguese speakers, who are usually underserved by indie
  puzzle games (full right-to-left support is a deliberate differentiator).

### 1.4 Unique selling points

- A real particle-based liquid simulation inside a 2048 merge game (most competitors use rigid circles).
- Ten modes, including head-to-head Battle over a local network and 26 hand-checked puzzles.
- Custom themes with their own colours, music and sounds, shareable as `.liqtheme` files.
- Shareable run codes that replay a whole game, on any device, with pause and speed controls.
- Zero monetization and zero internet requirement.

### 1.5 Comparable games

| Game | What we share | How Liq2048 differs |
|------|---------------|---------------------|
| Suika Game | Drop-and-merge on contact | Numbers instead of fruit; liquid particles instead of rigid circles |
| 2048 | Powers of two, merging | Physics and aiming instead of a swipe grid |
| Merge-style mobile clones | Core loop | No ads, no energy, local multiplayer, full RTL, custom themes |

---

## 2. Core gameplay

### 2.1 Core loop (seconds)

1. A droplet hovers at the top of the tank; the next one is shown in the NEXT box.
2. The player drags (or uses arrow keys) to choose the horizontal position. A guide line and a ghost circle show
   where it will land.
3. Releasing (or Space/Down) drops it. A 0.4 s cooldown prevents accidental double drops; a tap during the cooldown
   is buffered and fires as soon as it ends.
4. The droplet falls, splashes into the pile and settles. If it touches a droplet of the same number, the two
   merge into one bigger droplet with a surge of liquid.
5. Merges within 0.7 s of each other form a **chain** and multiply the score.
6. Repeat until the pile overflows (or the mode's goal or time limit is reached).

### 2.2 Session and meta loops

- **Session (2-10 min):** play a run, see the score, tap Restart.
- **Daily:** the Daily Challenge (same seed for everyone, UTC day) and a non-punitive streak.
- **Long term:** best scores per mode (bronze, silver, gold medals), 19 achievements, themes unlocked by
  achievements, 26 puzzles with 3 stars each, lifetime statistics, a custom theme to build.

### 2.3 Rules

| Rule | Value |
|------|-------|
| Droplet levels | 12 levels: 1, 2, 4 ... 2048 (level 0 to 11). 2048 does not merge further |
| Spawn weights | 1 (40 %), 2 (30 %), 4 (20 %), 8 (10 %) |
| Merge condition | Two droplets of the same level in contact (at least 2 particle contacts) |
| Overflow | At least 6 particles of a droplet above the dashed line, nearly at rest (and older than 1.2 s), for 2.0 s |
| Chain window | 0.7 s between merges |
| Score | `score += value of the new droplet x chain` |
| Drop cooldown | 0.4 s |

### 2.4 Controls

| Input | Action |
|-------|--------|
| Mouse / touch drag, release | Aim, drop on release (aiming works anywhere on the board) |
| Left/Right or A/D | Aim; Space or Down: drop |
| U or Z, undo button | Undo the last drop (Classic and Zen) |
| Esc, pause button | Pause menu (resume, restart, copy run code, settings, quit) |
| F11 | Fullscreen (desktop) |
| Mouse wheel | Nudge the aim |

Menus support full keyboard focus, mouse and touch. The game pauses by itself when the window loses focus.

### 2.5 Undo

Classic and Zen: 1 charge at the start of a run, +1 for every chain of 3 or more, up to 3.

---

## 3. Game modes

| # | Mode | Rules | Ends when | Notes |
|---|------|-------|-----------|-------|
| 1 | **Classic** | Endless | Overflow | Ranked best score; undo available |
| 2 | **Time Attack** | 90 seconds | Time out or overflow | |
| 3 | **Target** | Reach 256 within 140 drops; next run 512 (260 drops), then 1024 (520 drops) | Target reached, drops used, overflow | Escalating goals |
| 4 | **Daily Challenge** | Same seed for everyone each UTC day, 60 drops | Drops used and settled, or overflow | Always reference physics quality, no modifier |
| 5 | **Zen** | Gravity x0.35, no fail state | The player quits | Liquid stuck above the line drains away harmlessly |
| 6 | **Endless Rain** | Drops fall by themselves, every 3.2 s down to every 1.1 s; steering only | Overflow | Survive as long as possible; 60 s achievement |
| 7 | **Rising Tide** | The overflow line sinks 1.6 px/s up to 230 px (reached after about 2:24), then **keeps moving as a swell** (up and down by 22 px, 12 s period). Every merge lifts the line (more for bigger droplets) but never above its start | Overflow | 90 s achievement |
| 8 | **Weekly Challenge** | The same 100 drops for everyone for a whole week (Monday to Sunday, UTC) with that week's rule: low gravity, heavy gravity, thick liquid or bonus drops | Drops used or overflow | Own weekly best |
| 9 | **Puzzle** | 26 levels. Filled tank, a fixed drop queue, reach the goal droplet before the queue runs out | Goal reached or queue empty | 3 stars at par, 2 at par + 2, 1 for any solution |
| 10 | **Battle** | Timed duel (60, 90, 120 or 180 s). Both players get the identical queue in their own pot. Higher score wins; a tie goes to the higher droplet level; otherwise a draw | Time out or both pots finished | vs Bot (Easy, Medium, Hard, Adaptive), vs Human on the same Wi-Fi, or pass-and-play on one device |

### 3.1 Modifiers

Applied to the next Classic, Time Attack, Target, Zen, Rain or Tide run (and to both pots in Battle):

| Modifier | Effect |
|----------|--------|
| Low gravity | 55 % gravity |
| Heavy gravity | 140 % gravity |
| Thick liquid | 3x viscosity |
| Bonus drops | Bigger drops |

Runs with a modifier never change ranked best scores, but count for statistics and the "Modded 1000" achievement.

### 3.2 Battle details

- Same seed and queue for both players, so skill decides, not luck.
- If your pot overflows, your run ends early while the opponent keeps playing.
- LAN play: one device hosts, the other browses games on the same network and joins; the host presses Start.
  If the opponent disconnects, you win. Pausing a LAN match does not stop the clock.
- **View Opponent** toggles a live view of the other pot (never shown next to yours).
- Statistics: wins, losses and draws are kept and shown on the player card.
- Ghost pace: in solo modes the HUD can compare your score with your best run, drop by drop.

---

## 4. Physics and liquid design

This is the heart of the game, so it has its own section.

### 4.1 Model

Each droplet is a group of particles (more for bigger levels). The simulation is a position-based
particle fluid with a fixed timestep (1/60 s, 2 substeps by default, 1 to 3 selectable as "liquid quality"):

1. Gravity and drag integrate velocities.
2. **Double-density relaxation** (pressure plus near-pressure) keeps the liquid incompressible and prevents
   clumping.
3. **Viscosity** impulses between neighbours smooth the flow.
4. **Surface tension** (cohesion with the other particles of the same droplet) keeps a falling or splashing drop
   round and together. At rest it is switched off, so the liquid then levels like water.
5. Walls and floor are collision planes; particles slide along them.
6. Drops of the same level that touch (two or more close particle pairs) merge: their particles are re-seeded
   into one larger droplet with an outward splash.
7. Velocity is capped as a safety net.

The simulation is deterministic: it depends only on the step count and the recorded inputs, never on frame time.
That is what makes replays, the Daily Challenge and Battle fair.

### 4.2 Design goals for the liquid ("like real water")

| Real-water behaviour | How the game gets there |
|----------------------|-------------------------|
| Fast, free flow; low viscosity | Viscosity 60 (it was 300 before v1.7.4). The Thick modifier triples it |
| Surface levels flat when still | No surface tension at rest, low sleep damping: a settled pool is flat to within about 1 px |
| Waves and sloshing that travel and fade | Almost no artificial damping (0.9995 per substep); walls are nearly free-slip |
| Splashes, crowns and droplets on impact | Terminal speed raised to 520 px/s; surface tension only while moving |
| Incompressible | Double-density relaxation, 2 substeps |
| No jitter at rest | Median particle speed in a settled pool is about 0.5 px/s |

### 4.3 Measured results (v1.7.3 compared with v1.7.4)

Measured with an offline test rig on the same scenarios (a settled pool of mixed droplets, then a droplet dropped
from height):

| Measurement | v1.7.3 | v1.7.4 |
|-------------|--------|--------|
| Median particle speed in a settled pool (jitter) | 24.6 px/s | 0.5 px/s |
| Pool surface height range after settling | 10 to 15 px (lumps) | about 1 px (flat) |
| Peak velocity spikes | several thousand px/s | capped at 900 px/s |
| Detached splash particles after a high drop | 0 | up to 3 |
| Merges in a random-bot test | baseline | same (merge reach widened to match) |

These numbers come from measurements, not from listening or watching a human play; "feels like water" is still
a judgement the player makes.

### 4.4 Particle and droplet sizes

Radius grows by x1.15 per level from 17 px (`kBaseRadius`, `kRadiusGrowth`). Particle spacing is 4.4 px and the
interaction radius 9.6 px. Gravity is 600 px/s squared (Zen 0.35x, modifiers 0.55x and 1.4x).

### 4.5 Performance

- A performance governor halves the liquid substeps on slow devices (below about 40 fps) in modes where the physics
  need not be identical for everyone. Its changes are recorded in the run code so that replays stay exact.
- Daily, Puzzle and Battle always use the reference quality.
- Rendering uses a particle raster with dithered colour blending on a 270x480 canvas.

---

## 5. Replays and run codes

- Every run records its drops (step, aim position, droplet level), seed, gravity, liquid quality, modifier, the
  physics version, performance-governor changes and the **end step**.
- **Run code:** a short text (`LQ2-...`, base64url with a CRC-16) that can be copied at the end of a run **or from
  the pause menu** (the run from the first drop to that moment). Older `LQ1-` codes still load.
- **Watch a run code** (Additional features): replays the run in a pause-able viewer with play/pause, 1x/2x/4x
  speed and a progress bar. The replay always plays to the recorded end step.
- Puzzle runs have codes too (solution sharing and re-watching).
- A code made with a different physics version shows a warning, because the liquid may behave differently.
- Replays never change statistics, best scores or achievements.

---

## 6. Progression and economy

There is no currency and no store.

- **Best scores** per mode, with bronze, silver and gold medals.
- **19 achievements** (examples: first 128, 256, 512, 1024, 2048; chain x3 and x5; score 1000 and 5000; 1000
  drops; 3-day streak; modifier run; Rain 60 s; Tide 90 s).
- **Themes** (earned, never bought): Default; Steel (first 128); Sunset (chain x5); Forest (first 512). Each has its
  own colours, music and sound effects. **Custom theme** is available from the start.
- **Daily streak:** consecutive days played; it resets quietly, with no penalty, notification or expiring reward.
- **Statistics:** games, drops, merges, longest chain, highest droplet, time played, puzzles solved.
- **Player card:** a shareable PNG of the player's lifetime statistics with an editable name.
- **Backup / restore:** the save can be copied to the clipboard and restored.

---

## 7. Content and level design

### 7.1 Puzzles

26 levels, each a filled tank, a fixed queue and a goal. They were generated by a solver (`tools/puzzlegen`) that
searches for solvable positions and verifies the par number of drops by replay, so every level has a proven
solution. Difficulty rises along the ladder. Details: [PUZZLES.md](PUZZLES.md).

### 7.2 Difficulty management

- Droplet weights favour small levels so the pile grows slowly at first.
- Rain accelerates; Tide's line sinks and then swells; Target raises its goal after a win.
- Modifiers let players tune difficulty themselves.
- Undo (Classic/Zen) is the only forgiveness mechanic, limited and earned.

### 7.3 Tutorial and hints

The first game coaches inside the real tank (drag and release, same numbers merge, stay below the line) and is
skippable. The first time each mode is played, a one-line hint explains it.

---

## 8. User interface and flow

### 8.1 Screen map

```
Main menu: Start | Additional features | Progress | Settings
  Start -> Mode select (scrolling animated board) -> Game
          -> Battle menus (bot, host, join, pass-and-play) -> Game
          -> Puzzle ladder -> Game
  Additional features: Watch run code | Player card | Player name | Export theme | Import theme |
                       Backup | Restore | Authors | Quit
  Progress: Scores | Awards | Stats | History (charts)
  Settings: General (language, volume, frame rate, theme, Custom theme) | Game feel | Access
     Custom theme editor: Name | Colors | Sounds
Game: HUD (score, best, NEXT, undo, pause), pause menu, result screen
```

### 8.2 Layout

- A fixed 270x480 logical canvas scaled with nearest-neighbour filtering (crisp pixel art at any size),
  letterboxed with safe areas on phones.
- Portrait orientation, thumb-reachable controls; a left-hand HUD option mirrors the layout.
- All text uses a bundled pixel font (Latin) and Vazirmatn (Persian and Arabic); right-to-left languages mirror
  the layout and use an own Arabic shaper.

### 8.3 Feedback ("juice")

Squash and pop on merge, expanding rings, score pop-ups, chain banners, screen shake, slow motion on big merges,
particles, haptics on phones, merge and chain sounds that rise in pitch with the level. Calm mode and Reduced
motion turn these down or off.

### 8.4 Main menu animation

Droplets live in the menu, the mode cards are animated (two drops colliding for Battle, three drops moving for
Puzzle, a clock for Time Attack, a single quiet drop for Zen, and so on).

---

## 9. Custom themes

- **Editor** (Settings, "Custom theme"): English name (required), optional name in the current language (other
  languages fall back to English), all 34 colours with RGB sliders and a live preview, and a sound source for each
  of 12 slots (3 music loops, 9 effect groups): any built-in palette or the player's own WAV/OGG file.
- **Start from** any built-in theme.
- **`.liqtheme` file:** names, colours, music and sound effects in one file (documented binary container with
  CRC-32 and strict size limits). **Export / Import** live in Additional features; import also reads a file dropped
  on the window (desktop), the themes folder, or clipboard text (themes without audio files, handy on phones).
- The colour-vision and high-contrast transforms apply on top of any theme.

---

## 10. Audio

- Music: three loops per theme (menu, calm, intense), cross-faded by mood and by how full the tank is.
- Sound effects: 27 per theme (drops, splashes, 12 merge pitches, chain 2 to 6, click, swipe, fail, success, unlock).
- Four sound palettes, one for each built-in theme: Default (A minor, mellow pentatonic), Steel (D dorian,
  metallic bars), Sunset (F major, soft marimba and long echo), Forest (E minor, log drums, bongos and
  wood-like merges).
- All sounds are synthesized offline by `tools/audiogen` (nothing is recorded or sampled), saved as 24-bit FLAC
  masters in `original_assets/` and optimized OGG in `assets/`; both CC BY 4.0.
- Separate music and effects volumes. Every sound has a visual counterpart.

---

## 11. Art direction

- Flat pixel art on a 270x480 grid; a deep indigo night sky with stars; a golden frame around the tank.
- Droplets are liquid blobs: the particle field is rasterized with a smooth threshold and dithered colour blending,
  so droplets merge visually as they merge physically. Each level has its own colour and a number label.
- 12 droplet colours per theme, ordered by lightness; alternate palettes for colour-vision deficiencies with
  maximised minimum colour distance (about 22 CIE Lab against 3 to 8 for the standard palette).
- Icons and fonts are pixel-drawn (fonts are the external BDF files `pixel-5x7.bdf` and `pixel-3x5.bdf`).

---

## 12. Accessibility and localization

See [ACCESSIBILITY.md](ACCESSIBILITY.md) and [LOCALIZATION.md](LOCALIZATION.md). Summary:

- Colour-vision palettes (red-green, blue-yellow), high contrast, big touch areas, left-hand HUD.
- Reduced motion, Calm mode, optional shake, slow motion and vibration.
- Number on every droplet; sound never carries information alone.
- 8 languages: English, Persian, Spanish, French, German, Turkish, Portuguese, Arabic (two right-to-left).
  String tables are JSON; a test requires every key to exist in every language.
- Known gaps: screen-reader labels, large text and Cyrillic/CJK/Devanagari scripts (planned).

---

## 13. Technical design

### 13.1 Architecture

```
src/core     Config, Types, Themes, CustomTheme, Save, Platform, App loop
src/game     World (fluid), Mode (rules), Replay, Puzzles, Battle (bot, LAN)
src/gfx      Gfx, particle Raster, PixelFont (BDF), ArabicShaper, Png, Icons
src/audio    Audio (SDL mixer, OGG/WAV decoding, per-theme packs)
src/i18n     Loc (JSON tables, RTL)
src/scenes   Menu, Modes, Game, Battle, Extras, Settings, ThemeEditor, Credits
tools        audiogen (sound synthesis), puzzlegen (puzzle search)
tests        unit and regression tests (CTest)
```

- **Scenes:** a small scene stack; each scene draws and handles input in `frame(Ctx&, dt)`.
- **Modes:** one `Mode` strategy class per game mode (rules, seeds, drop limits, gravity scale, status text).
- **Determinism:** fixed 1/60 s step; everything that affects physics (tide line, overflow timers, performance
  substeps) happens on step boundaries.
- **Save:** key=value text with clamped values and a backup/restore path; the custom theme lives in
  `custom.liqtheme`.
- **Networking:** local-network Battle only, started only when the player opens the LAN screens. No servers.
- **Assets:** loaded at run time from `assets/` (sounds, fonts, language files); everything else is drawn in
  code.

### 13.2 Build and platforms

CMake, SDL2, SDL2_ttf. Desktop builds on Linux, Windows and macOS (CI on all three), Android through Gradle with
the SDL2 sources, iOS through an Xcode project. Details: [BUILDING.md](BUILDING.md).

### 13.3 Quality

- More than 40 unit and regression tests: physics determinism and replay checksums, mode rules, save clamping,
  localization completeness, audio decoding, Arabic shaping, theme file parsing (including corrupt and forged files),
  run codes, Battle logic.
- Compiler warnings are errors in CI. CodeQL scans the code.
- A headless screenshot tool renders any scene for visual checks.

### 13.4 Performance budget

60 fps on mid-range phones (the governor lowers liquid quality below about 40 fps); the whole game is well under
100 MB with all sound palettes (the optimized sounds are about 5 MB).

---

## 14. Monetization and ethics

None. The game contains no ads, in-app purchases, paid power-ups, energy timers, loot boxes, analytics or tracking,
and never uses the internet. The roadmap has no store-release plan. Progress rewards are earned, never sold.

---

## 15. Risks and mitigations

| Risk | Mitigation |
|------|-----------|
| Physics feels "too thick" or "too loose" | All constants in `Config.hpp`; measured test rig; the Thick modifier for those who want syrup |
| Replays diverge between versions | Physics version stored in every code; warning on mismatch |
| Slow phones | Performance governor; liquid quality setting; fixed 3-step catch-up limit |
| Translations are not native-reviewed | Strings kept short and plain; a test guarantees completeness |
| Custom files could be malicious | Strict parser with size limits and CRC; audio decoders bounded in length |
| Mobile file access (no file picker) | Themes folder plus clipboard text for themes without audio files |

---

## 16. Production history (changelog summary)

| Version | Theme |
|---------|-------|
| 1.0 | Core game: Classic, Time Attack, Target, Daily, Zen; English and Persian |
| 1.1 | Feel: undo, haptics, Calm mode, auto-pause |
| 1.2 | Reach: accessibility and 8 languages |
| 1.3 | Modes: Rain, Tide, Weekly, Puzzle, Battle groundwork, modifiers |
| 1.4 | Liquid: simulation and visuals, second pass |
| 1.5 | Social without a server: run codes, share results |
| 1.6 | Craft: input, desktop and engineering quality |
| 1.7.0 | Battle: head-to-head play with bot or LAN, more puzzles |
| 1.7.1 | Menus and touch: four-button menu, Additional features, animated mode board, player card, backup, mobile fixes |
| 1.7.2 | Pass-and-play, ghost pace, history charts, result image, checksums |
| 1.7.3 | Custom themes, per-theme sound, `.liqtheme`, external fonts, licences |
| 1.7.4 | Water-like liquid, replay fixes (pause, speed, full playback), Tide keeps moving, copy run code from the pause menu, puzzle run codes, this document |

### Roadmap (see [ROADMAP.md](ROADMAP.md))

- **1.8.0:** mirror mode, true ghost replay, large-text option, safe-area handling, 19 more puzzles,
  screen-reader labels.
- **2.0.0 ideas:** gamepad support, more scripts and languages, mod packs.

---

## 17. Glossary

| Term | Meaning |
|------|---------|
| Droplet / blob | A numbered liquid body made of particles |
| Level | Droplet size index 0 to 11 (the number shown is 2^level: 1 to 2048) |
| Chain | Merges within 0.7 s of each other |
| Overflow line | The dashed line near the top of the tank |
| Run code | Text that stores a whole game so it can be replayed |
| Palette | One of the four built-in sets of music and sound effects |
| `.liqtheme` | File that stores a custom theme (names, colours, sounds) |
