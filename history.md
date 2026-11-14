# Liq2048 — history

## 1. Where the idea came from

The idea is a mix of two games everyone knows. **Suika Game** gave the loop: hold a piece, choose where to drop it,
and when two equal pieces touch they become one bigger piece. **2048** gave the numbers: 1 + 1 = 2, 2 + 2 = 4, up
to 2048. The design document states the question the project started from: *what if the pieces were not rigid balls?*
Almost every merge game uses solid circles. Liq2048 makes each droplet a small cluster of fluid particles, so
pieces squash, slosh, settle and spill, and the player has to read a *liquid*, not a pile of marbles.


The design goals written down on day one (GDD): a one-handed, short-session game; offline; no monetisation; small and
self-contained; and readable for everyone (8 languages including right-to-left Persian and Arabic, colour-vision
palettes, reduced motion).

## 2. Why C++17 and SDL2

These reasons are the ones the project's own documents give:

- **Particle physics needs control of memory and speed.** The liquid is a few hundred to a thousand particles stepped
  many times per frame on phones. C++ lets the solver live in flat arrays with a counting-sort neighbour grid, no
  garbage collector pauses, and tight loops the compiler can vectorise.
- **Determinism.** Replays, run codes, the daily/weekly seed and LAN Battle all depend on the simulation giving the
  same result from the same inputs on every platform. A fixed-step solver written in plain C++ with no fast-math is
  something we can test and hash.
- **One code base for six platforms.** Windows, macOS, Linux, Android and iOS from a single CMake project. SDL2
  provides window, input, touch, audio and a GPU texture path without imposing an engine.
- **Small and self-contained.** The executable draws everything in code and ships a handful of assets; there is no
  engine runtime, no store SDKs, no network by default.
- **Owning the whole stack.** No engine means the physics, UI toolkit, localisation and shaping (Arabic/Persian) are
  ours and can be unit-tested; the test suite links the same `liq_core` library the game runs.

## 3. The shape of the project

`liq_core` is a static library; `main.cpp` only parses the command line. Layers (checked by `tools/check_layers.py`):
`core` → `gfx`/`i18n`/`audio`/`net` → `game` (rules and physics) → `save`/`battle` → `ui` → `scenes`.
See `docs/ARCHITECTURE.md` for the diagram.

## 4. Release timeline

| Version | Theme |
|---|---|
| 1.0.0 | First playable: physics blob-merging, pixel-art look |
| 1.1.0 | Feel: undo, haptics, calm mode |
| 1.2.0 | Reach: 8 languages, colour-vision palettes, high contrast |
| 1.3.0 | Modes: Rain, Tide, Weekly, Puzzle, modifiers |
| 1.4.x | Liquid quality, determinism test, replay recorder |
| 1.5.0 | Run codes and result sharing |
| 1.6.x | Gamepad, fuzzing and sanitizer CI, Persian/Arabic shaping |
| 1.7.x | Battle mode (bot + LAN), pass-and-play, custom themes |
| 1.8.x | Game-mode revolution, liquid types, modular scenes |
| 1.9.0 | Mobile-first, security hardening |
| 1.9.1–1.9.4 | HUD fixes, image export, harder game, multi-step undo, CI build fixes |
| 1.9.8–1.9.11 | Finger-sized touch, liquid-jump fix, obstacle pots, mazes, soak tests, predetermined drop bags |
| 1.9.12 | Performance and security pass, layer checks, bigger default touch, this history |

## 5. Lessons the project taught

- **A stable solver beats a clever one.** In 1.9.9 a pressure push with no upper bound made liquid blobs jump when
  squeezed. The fix was a clamp on push distance, a cap on the jelly spring and at least two substeps for jelly
  liquids — and a soak test (1.9.10) so it cannot silently return.
- **Three compilers, three opinions.** Clang, MSVC and Apple clang each rejected code the others accepted (1.9.4).
  CI on all three, with warnings as errors, is now the rule.
- **Phones are not small desktops.** A 15 px button is about 4 mm on a phone. Hit areas grew in 1.9.8 and again in
  1.9.12; the nearest-centre rule decides when two margins overlap; undo no longer leaks a tap into the board.
- **Determinism is a feature you pay for daily.** Replay codes store explicit drops, so changing the drop generator
  (1.9.3, 1.9.11) did not break old codes; the physics version number changes only when the physics does.
- **Predetermined drops are planning.** From 1.9.11 the next three drops are known, dealt from bags of six pairs, so
  a good player can plan instead of reacting.

## 6. Diary

One entry per working day, taken from the git history (a day with several releases shows all of them).

### Day 1 — 2026-10-31
- feat: Liq2048 1.0.0 - physics blob-merging puzzle in C++17/SDL2

### Day 2 — 2026-11-01
- feat(gfx): pixel-art visual overhaul with fused droplets and FPS setting

### Day 3 — 2026-11-02
- feat(physics): replace rigid blobs with a particle liquid simulation
- feat(audio,physics): procedural sound engine, settling liquid, impact splashes
- feat(release): Android and iOS builds, self-contained desktop packages, v1.1.0
- feat(audio): ship sound as OGG files in assets/, move synth to tools/audiogen; repo cleanup
- release: v1.0.0 stable, add roadmap and tagging plan
- release: v1.1.0 Feel - undo, haptics, calm mode, auto-pause, window icon

### Day 4 — 2026-11-03
- release: v1.2.0 Reach - 8 languages, colour-vision palettes, high contrast, reduced motion, left-hand HUD
- release: v1.3.0 Modes - Rain, Tide, Weekly, Puzzle, modifiers, stats, awards, coach hints
- fix(build): resolve CMake configuration errors
- fix(core): remove unused world seed field
- fix(build): resolve SDL2 linking issues across platforms
- fix(build): resolve MSVC compiler warnings in menu scenes
- fix(ci): remove deprecated Android SDK tools package
- fix(android): resolve SDL2 and SDL2_ttf dependency configuration
- release: v1.4.0 Liquid (quality setting, faster physics step, determinism test)
- release: v1.4.1 Replay (recorder, Watch replay, determinism test)
- release: v1.5.0 Run codes and result sharing

### Day 5 — 2026-11-04
- release: v1.6.0 Craft (gamepad, mouse wheel, F3 overlay, fuzz and sanitizer CI)
- release: v1.6.1 (Persian/Arabic shaping and bundled font, mobile layout, harder game, Rising Tide, authors page, Battle roadmap)

### Day 6 — 2026-11-05
- v1.7.0: Battle mode (bot + LAN), View Opponent, new main menu, 11 more puzzles
- v1.7.1: classic menu, Additional features, animated mode board, player card, mobile touch fixes

### Day 7 — 2026-11-06
- v1.7.2: pass-and-play, ghost pace, history charts, result image, checksums

### Day 8 — 2026-11-07
- v1.7.3: custom themes, per-theme sound, external fonts, licences
- v1.7.3: lossless audio masters (default)
- v1.7.3: lossless audio masters (steel)
- v1.7.3: lossless audio masters (sunset)
- v1.7.3: lossless audio masters (jungle)
- v1.7.4: replay fixes, water-like liquid, Tide swell, copy run code, GDD
- v1.8.0 Game Mode Revolution: Mirror, Big Container, Madhouse, I Don't Know; Wind/Quake/Fog; Rain and Tide became modifiers; new mode board
- v1.8.1 Liquid settings: liquid quality and types (Water, Dilute, Oil, Honey, Slime, Mercury); host's liquid in LAN matches

### Day 9 — 2026-11-09
- v1.8.2: modular scenes, widget library, faster physics, smaller binary

### Day 10 — 2026-11-10
- v1.8.3: simple mode list, mode page with preview clips, tabbed settings, Mirror removed
- v1.8.4: harder Tide, Liquid mixing, combo meter, Reserve, photo mode, result picture, liquid haptics

### Day 11 — 2026-11-11
- v1.8.5: quick resume, first-run coach, Big Container co-op, picture profile
- v1.9.0: mobile-first (touch input fix, queue, pause in Big Container, texture upload, tilt maze), security hardening

### Day 12 — 2026-11-12
- v1.9.1: HUD overlap and layering fixes
- v1.9.2: save photos/cards to Pictures and copy the image to the clipboard
- v1.9.3: harder in all modes, multi-step undo to before the last combination, remove Reserve
- v1.9.4: fix macOS, clang and MSVC build errors

### Day 13 — 2026-11-13
- v1.9.8: bigger touch targets, undo no longer drops, Tilt maze on the mode screen
- v1.9.9: fix liquid jumping (bounded pressure push, capped jelly spring)

### Day 14 — 2026-11-14
- v1.9.10: obstacle pots, more maze kinds, physics soak and touch simulation tests
- v1.9.11: predetermined drop bags with three-drop lookahead
