# Liq2048 roadmap

Liq2048 **1.0.0** is the first stable release. This roadmap covers everything planned after it, grouped into
**stations**: each station is a coherent, shippable step. When a station is finished (all exit criteria met, CI
green, `CHANGELOG.md` updated) the closing commit gets an annotated git tag, `vX.Y.Z`, and the Release workflow
publishes it. Smaller fixes in between ship as patch tags (`v1.0.1`, ...).

The project promises stay fixed in every station: **free, open source, no ads, no purchases, no tracking, no
internet access.** (From v1.7.0 the only networking is the optional, local-network Battle mode, started by the player.)

Status legend: ☐ planned, ◐ in progress, ☑ done.

```
v1.0.0 ─ v1.0.x ─ v1.1.0 ─ v1.2.0 ─ v1.3.0 ─ v1.4.0 ─ v1.5.0 ─ v1.6.0 ─ v1.7.0 ─ v1.8.0 ─ v2.0.0
 stable   verify   feel    reach    modes    liquid   social   craft    battle    modding   next gen
```

---

## v1.0.x: Verification and hotfixes

Goal: prove that everything 1.0.0 claims actually works on real devices, and fix what does not.

- ☐ Push the `v1.0.0` tag and confirm the Release workflow produces all five platform packages.
- ☐ First real run of the Android job (APK + AAB) and iOS job (unsigned IPA); fix Gradle/NDK/Xcode issues.
- ☐ Smoke-test installs on: Windows 10/11, Ubuntu LTS, macOS (Apple Silicon and Intel), a low-end Android
  phone (API 24), a recent Android phone, an iPhone via sideloading.
- ☐ Listening pass over all 27 sound effects and 3 music loops: levels, loop seams, clipping on phone speakers;
  re-run `tools/audiogen` with fixes.
- ☐ Performance budget check on a low-end phone: 60 fps with a full pot, battery draw over 30 minutes.
- ☐ Persian text and RTL review by a native reader on all scenes.
- ☐ Save-file safety: corrupted/partial save never crashes; versioned save format with migration hook.
- ☐ Crash-free soak test: 2 hours of automated play with random input in CI (nightly).

Exit: all five artifacts install and run; no known crash; tag `v1.0.1` (or later patch).

## v1.1.0: Feel (game feel and UX polish) ☑ released

Goal: make every touch and merge feel great.

- ☑ Haptics on Android (vibration permission only) with a setting; iOS uses SDL's haptic support where available.
- ☑ Aim guide on/off (dotted line and landing ring).
- ☑ Undo (one step back; 1 charge per run, +1 per chain of 3 or more, max 3) in Classic and Zen.
- ☑ Auto-pause when the window loses focus or the app goes to the background.
- ☑ Screen shake toggle, slow motion on big merges toggle, and **Calm mode** (no shake, slow motion, blinking or
  flashes, half the particles).
- ☑ Larger accent-coloured score pop-ups for chains of 3 or more.
- ☑ Window and taskbar icon on desktop from the logo (`assets/icon.bmp`).
- ☑ Settings split into two pages (General / Game feel).
- ➜ Moved: interactive tutorial to v1.3.0; safe-area handling, audio ducking for calls, aim snapping and
  preview opacity to v1.6.0 (see those stations).

Exit: tag `v1.1.0`.

## v1.2.0: Reach (accessibility and languages) ☑ released

Goal: playable by as many people as possible.

- ☑ Colour-blind-safe droplet palettes (Red/Green for protan/deutan, Blue/Yellow for tritan), found by a
  distinguishability search under colour-blindness simulation (min distance about 22 vs 3 to 8 standard).
  Droplets already carry numerals, so level never depends on colour alone; extra pattern marks were not needed.
- ☑ High-contrast UI option; invisible touch margins on phones plus a "Big touch areas" option.
- ☑ Left-hand HUD (mirrored layout).
- ☑ Reduced-motion mode (instant scene changes, no particles, rings, bubbles, shake or drifting backdrop).
- ☑ In-game language picker with 8 languages: English, Persian, Spanish, French, German, Turkish, Portuguese,
  Arabic. A test checks that every language has every key; workflow in `docs/LOCALIZATION.md`.
- ☑ Right-to-left audit extended to Arabic (screenshots checked with the Vazirmatn font).
- ☑ `docs/ACCESSIBILITY.md` with the option table and a checklist for new features.
- ➜ Moved to v1.3.0: interactive tutorial with the real pot.
- ➜ Moved to v1.6.0: screen-reader labels (TalkBack, VoiceOver), large-text option, notch/safe-area handling,
  audio ducking for calls, aim snapping and preview-opacity options, lowered one-handed HUD.
- ➜ Moved to v1.8.0: Russian, Chinese, Japanese, Hindi and other scripts (need bundled fonts and a TTF face
  for non-Arabic scripts), per-language plural rules; best done with community translators.

Exit: tag `v1.2.0`.

## v1.3.0: Modes (more ways to play) ☑ released

Goal: more reasons to come back, still without any network.

- ☑ **Endless Rain**: drops fall by themselves, faster over time; personal best only.
- ☑ **Puzzle**: 15 levels (generated and verified by a solver, see `docs/PUZZLES.md`) with pre-filled pots, a fixed
  queue, a goal tile and three stars.
- ☑ **Rising Tide**: the overflow line sinks; merging pushes it back up.
- ☑ Weekly challenge on top of Daily (fixed seed per week, special rule).
- ☑ Modifiers: low gravity, heavy gravity, thick liquid, bonus drops.
- ☑ Statistics tab (games, drops, merges, longest chain, highest tile, time, puzzles) and ten more achievements
  (19 total) with bronze/silver/gold medals per mode.
- ☑ Interactive first-run coach inside the real pot, plus a one-time hint per mode.
- ➜ Moved to v1.5.0: **Mirror** (two pots, one drop stream) together with hot-seat versus, which also needs two
  worlds; per-mode history charts.
- ➜ Moved to v1.4.0: the "tiny pot" modifier, together with pot shapes.
- ➜ Moved to v1.8.0: more puzzle packs (the planned 60 levels; pack 1 has 15) and a level editor.

Exit: tag `v1.3.0`.

## v1.4.0: Liquid (simulation and visuals, second pass)

Goal: push the liquid to its best while keeping 60 fps on a low-end phone.

- ✔ Physics performance: precomputed pressures and flags, no hypot in hot loops, spatial-hash scan limited to occupied
  cells. Measured with the same scene on the same machine: 1.63 → 1.50 ms/step (about 8%). The 2x target was NOT met;
  SIMD and threads are moved to a later milestone.
- ✔ "Liquid quality" (Low / Medium / High = 1 / 2 / 3 substeps) in Settings → Game feel. Daily and Puzzle always use Medium
  so every player gets the same physics.
- ✔ Test: the same inputs give bit-identical results within one build, at every quality level.
- ✔ (v1.4.1) Replay recorder: every drop is logged (physics step, position, level); "Watch replay" on the game-over
  screen replays the run without touching saves or achievements. Physics is reproduced exactly; timers (Timed, Rain,
  Tide) follow wall-clock time, so those modes replay approximately. A checksum test covers playback and the text format.
- ➜ Moved to v1.5.0 or later: foam/bubble sprites, marbling colour setting, saving/sharing replays as files, cross-platform
  determinism check (replay checksums are only verified on the build that produced them), 6 new themes and animated backdrops, pot shapes and the "tiny pot" modifier.

Exit: tag `v1.4.0` (a smaller release than planned; the moved items keep their place on the roadmap).

## v1.5.0: Social without a server

Goal: sharing and friendly competition, with zero network code in the game.

- ◐ Share card: the **player card** (lifetime stats) is saved as a PNG file and its text copied (v1.7.1). The final-pot card and the system share sheet are not done.
- ✔ Run codes: "Copy code" on the game-over screen puts a short `LQ1-…` code (drops + settings, checksummed) on the clipboard; "Watch run code" in the menu replays it. Racing a code is not done yet.
- ✔ Result text ("Liq2048 V1.5.0 - Classic: 1024 pts, 38 drops, chain x4") copied with "Copy result". It has no date yet.
- ☐ Local hot-seat versus mode: two players alternate drops in one pot, score by merges.
- ☐ **Mirror** mode (two pots, one drop stream, alternate sides; from v1.3.0) and per-mode history charts on the
  Stats tab.
- ☐ Ghost races against your own best replay.
- ✔ (v1.7.1) Save backup and restore through the clipboard and a backup file in the save folder. A file picker is not done.

Status: partial. Share card PNG, hot-seat versus, Mirror mode, history charts, ghost races and save import/export are moved to v1.5.x. Clipboard use is untested on mobile.

Exit: all sharing paths work on all five platforms without any permission beyond the share sheet. Tag `v1.5.0` (first part).

## v1.6.0: Craft (input, desktop and engineering quality)

Goal: a solid, pleasant foundation.

- ✔ Gamepad support (SDL GameController): d-pad/stick aims, A drops/confirms, B/Start/Back pauses, X undoes, d-pad navigates menus. Written and compiled, NOT tested with a real controller; no Steam Deck checklist yet.
- ✔ Mouse wheel nudges the aim. ➜ Moved: remappable keys, full keyboard audit.
- ☐ Desktop polish: resizable window with integer scaling, borderless fullscreen, multi-monitor, high-DPI.
- ☐ Controller/keyboard glyph hints in the UI.
- ✔ Fuzz smoke tests (JSON, save loader, run codes, replay text, OGG) and an ASan + UBSan CI job; clean locally. Found a leak in third-party stb_vorbis on corrupt OGG (suppressed in `tests/lsan.supp`, shipped sounds unaffected). ➜ Moved: coverage report with a floor.
- ☐ Reproducible builds and checksums (`SHA256SUMS`) attached to every release; signed tags.
- ✔ F3 toggles a frame-time graph (particle count not shown yet).
- ☐ Carried over from v1.1.0/v1.2.0: screen-reader labels (TalkBack, VoiceOver), large-text option,
  notch/safe-area handling, audio ducking for calls, aim snapping and preview-opacity options, lowered
  one-handed HUD.
- ☐ Source documentation pass: every header has a one-paragraph purpose comment; `docs/ARCHITECTURE.md` diagrams.

Status: partial; the rest is moved to v1.6.x. The new CI job has not run on GitHub yet.

Exit: CI includes sanitizers and fuzz smoke tests, all green. Tag `v1.6.0` (first part).

## v1.7.0: Battle (head-to-head play and more puzzles)

Goal: play against someone, or against the game, for the highest score in a fixed time.

- ✅ **Battle mode** (new mode, timed): every player gets an identical drop queue (same seed) in their own pot, and the
  player with the highest score when the clock runs out wins. Ties are broken by the highest level reached.
  Modifiers apply; 60/90/120/180 s.
- ✅ **PvE**: Versus Bot with Easy, Medium, Hard and Adaptive difficulty (human-like aim delay and mistakes), offline.
- ✅ **PvP over LAN**: host a game, others find it in *Browse games* (UDP broadcast), the host sees the join and
  presses Start. No internet, no account; networking starts only on the LAN screens.
- ✅ **View Opponent** toggle: the opponent's pot streamed as ~10 Hz particle snapshots and interpolated, so it is exact
  on every platform without needing cross-platform determinism (each player's score is computed on their own device).
- ✅ Result screen with scores, win/loss/draw record and rematch.
- ✅ New main menu with Classic, Battle, Puzzle and Zen as the four main tiles.
- ◐ **More puzzles**: 15 → 26 levels, all solved and verified with `tools/puzzlegen`. The target of 45 (and new mechanics
  such as walls and locked drops) is **not** reached: the generator needs minutes per hard level and found nothing for
  the hardest slice. Moved to v1.8.0.
- → Moved to v1.8.0: hot-seat PvP on one device; the remaining 19 puzzles; per-level star targets.
- ☐ Not yet verified: LAN play on real Wi-Fi between two physical devices, Android/iOS permission behaviour (iOS local
  network prompt, broadcast on iOS), bot balance with real players, and native-speaker review of the Battle translations.

Exit: a two-player LAN match and a PvE match both play to the end on desktop and Android. Tag `v1.7.0`.

## v1.7.1: Menus and touch

- ✔ Classic main menu (Start, Additional features, Progress, Settings), Additional features screen, scrollable animated mode board.
- ✔ Player card PNG, player name, save backup and restore.
- ✔ Touch: buffered drops, tolerant tap release, performance governor (not measured on a device).
- ➜ Everything not built so far moved to v1.7.2.

## v1.7.2: Everything still open

- ✔ Pass-and-play versus (two players alternate drops in one pot).
- ✔ Ghost pace against your best run (a score-pace readout, not a second pot).
- ✔ History charts (last 12 scores per mode).
- ✔ Share image of the final pot (file only; no system share sheet).
- ✔ Desktop: borderless fullscreen (F11 / setting) and integer scaling already existed; key hints added to the pause menu.
- ✔ `SHA256SUMS` in release CI (unverified until the next tagged release runs).
- ☐ Not built, moved to v1.8.0: **Mirror** mode (needs two pots in one scene), a real ghost replay drawn in the pot,
  large-text option and notch/safe-area handling, the remaining 19 puzzles (26 of 45), screen-reader labels, controller glyph art.

## v1.7.3: Custom themes and sound ☑ released
- ☑ Custom theme editor (name, colours, sounds), `.liqtheme` import and export.
- ☑ Own music and sound effects for each built-in theme; masters in `original_assets/`, optimized OGG in `assets/`.
- ☑ Fonts loaded from external files; sounds and Liq Pixel under CC BY 4.0.

## v1.7.4: Replay and water ☑ released
- ☑ Replay viewer: pause, speed, progress, exact full playback; step-based, deterministic timing.
- ☑ Rising Tide keeps moving; water-like liquid physics; copy run code from the pause menu; puzzle run codes.
- ☑ Game design document (`docs/GDD.md`).

## v1.8.0: Modding and community

Goal: let players and artists extend the game with no code.

- ☐ Data-driven themes: a theme is a JSON file plus optional palette image; loaded from a `mods/` folder.
- ☐ Data-driven puzzle packs and rule modifiers (JSON), validated with clear error messages.
- ☐ Custom sound packs: drop OGG files with the standard names into `mods/sounds`.
- ☐ In-game mod list with enable/disable; mods can never touch the save file or run code.
- ☐ Level editor (desktop first): place drops, set goals, test-play, export a puzzle pack.
- ☐ Community docs: `docs/MODDING.md`, contribution guide, "good first issue" labels, translation guide,
  issue and PR templates refreshed, Discussions enabled.
- ☐ Showcase page listing community packs (links only, no hosting).
- ☐ More languages and scripts (Russian, Chinese, Japanese, Hindi, ...) with bundled OFL fonts and a TTF face
  for non-Arabic scripts; per-language plural rules (carried over from v1.2.0).

Exit: three example mods shipped in the repo and loaded by tests. Tag `v1.8.0`.

---

## v2.0.0: Next generation (long-term ideas)

Larger changes that may justify a major version. Each is optional and will be decided by player feedback.

- ☐ Optional 3D-lit "deluxe" renderer (kept off by default; the pixel look stays the identity).
- ☐ Campaign: a map of regions, each with a puzzle pack, boss pots and a story told in a few lines of text.
- ☐ Procedural music with more variation: stems layered by game state, new key and mood per theme.
- ☐ Tablet and foldable layouts, desktop widescreen layout with side panels.
- ☐ Console-style couch mode with controller-first UI.
- ☐ Switch of the engine core to a standalone library (`liq_core`) that other projects can embed.

---

## Standing tracks (every station)

| Track | What happens in each station |
|-------|------------------------------|
| Quality | New code comes with tests; CI stays green on Linux, macOS, Windows; warnings as errors. |
| Performance | Frame-time and memory numbers recorded in the station's release notes. |
| Docs | `CHANGELOG.md`, `README.md`, affected `docs/*.md` and screenshots updated before the tag. |
| Localization | New strings added to `en.json` and `fa.json` at least; missing keys fall back to English. |
| Security and privacy | Dependencies updated, CodeQL clean, no internet access (only the optional local-network Battle mode) and zero data collection. |
| Audio | New sounds added through `tools/audiogen`, never as unexplained binaries. |

## Release and tagging procedure

1. All items of the station are ☑ (or explicitly moved to a later station in this file).
2. `CHANGELOG.md`: move *Unreleased* into the new version with the date; update the compare links.
3. Bump `project(... VERSION x.y.z)` in `CMakeLists.txt` and `liqVersion` in `android/app/build.gradle`.
4. CI green on `main`; commit message `release: vX.Y.Z`.
5. Create the annotated tag on that commit and push it:
   ```bash
   git tag -a vX.Y.Z -m "Liq2048 X.Y.Z: <station name>"
   git push origin main vX.Y.Z
   ```
6. The Release workflow builds and publishes Windows, macOS, Linux, Android and iOS packages.
7. Never move or delete a pushed tag; fix forward with a patch tag.

Version rules: patch = fixes only; minor = a finished station with new features; major = changes that break
saves, replays or mods (with a migration path).

## Tag map

| Tag | Station | Status |
|-----|---------|--------|
| v0.1.0 | Playable prototype | ☑ |
| v0.2.0 | Pixel-art overhaul | ☑ |
| v0.3.0 | Liquid simulation | ☑ |
| v0.4.0 | Sound and settling physics | ☑ |
| v0.5.0 | Mobile builds and packages | ☑ |
| **v1.0.0** | **Stable release** | ☑ |
| v1.0.x | Verification and hotfixes | ☐ |
| v1.1.0 | Feel | ☑ |
| v1.2.0 | Reach | ☑ |
| v1.3.0 | Modes | ☑ |
| v1.4.0 | Liquid, second pass (partial) | ✔ |
| v1.5.0 | Social without a server (first part) | ◐ |
| v1.6.0 | Craft (first part) | ◐ |
| v1.6.1 | Fixes: Persian/Arabic, mobile layout, difficulty, Rising Tide, authors page | ☑ |
| v1.7.0 | Battle (PvP over LAN, PvE) and more puzzles | ✅ (26 of 45 puzzles) |
| v1.7.1 | Menus and touch | ✅ |
| v1.7.2 | Everything still open | ✅ (partly; see above) |
| v1.8.0 | Modding and community | ☐ |
| v2.0.0 | Next generation | ☐ |

Suggestions and priorities are welcome as GitHub issues and Discussions; the order of stations can change when
players ask for something else first.
