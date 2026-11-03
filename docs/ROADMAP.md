# Liq2048 roadmap

Liq2048 **1.0.0** is the first stable release. This roadmap covers everything planned after it, grouped into
**stations**: each station is a coherent, shippable step. When a station is finished (all exit criteria met, CI
green, `CHANGELOG.md` updated) the closing commit gets an annotated git tag, `vX.Y.Z`, and the Release workflow
publishes it. Smaller fixes in between ship as patch tags (`v1.0.1`, ...).

The project promises stay fixed in every station: **free, open source, no ads, no purchases, no tracking, no
network access in the game itself.**

Status legend: ☐ planned, ◐ in progress, ☑ done.

```
v1.0.0 ─ v1.0.x ─ v1.1.0 ─ v1.2.0 ─ v1.3.0 ─ v1.4.0 ─ v1.5.0 ─ v1.6.0 ─ v1.7.0 ─ v1.8.0 ─ v2.0.0
 stable   verify   feel    reach    modes    liquid   social   craft    stores    modding   next gen
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

## v1.3.0: Modes (more ways to play)

Goal: more reasons to come back, still without any network.

- ☐ **Endless Rain**: a calm survival mode where drops keep coming faster; leaderboard-free, personal best only.
- ☐ **Puzzle**: hand-made starting pots with a goal ("reach 512 in 12 drops"); first pack of 60 levels, three
  stars each, level editor data format documented.
- ☐ **Rising Tide**: the floor slowly rises; merging lowers it.
- ☐ **Mirror**: two pots, one drop stream, alternate sides.
- ☐ Weekly challenge on top of Daily (fixed seed per ISO week, special rule).
- ☐ Modifiers for any mode: low gravity, thick (viscous) liquid, tiny pot, bonus drops.
- ☐ Statistics page: drops, merges, longest chain, time played, per-mode history charts.
- ☐ Ten more achievements and a medal for each mode.
- ☐ Interactive 20-second tutorial with the real pot (carried over from v1.1.0).

Exit: every mode has a tutorial card, a best-score record and tests for its rules. Tag `v1.3.0`.

## v1.4.0: Liquid (simulation and visuals, second pass)

Goal: push the liquid to its best while keeping 60 fps on a low-end phone.

- ☐ Physics performance: SIMD (SSE/NEON) neighbour loops, tuned spatial hash, optional multi-threaded substeps;
  target a 2x cheaper step so the particle count can rise.
- ☐ Higher-fidelity option ("Liquid quality": Low / Medium / High) that changes particle spacing and substeps.
- ☐ Surface tension and foam/bubble sprites on hard impacts; droplets that detach and re-merge cleanly.
- ☐ Colour mixing refinement: deterministic mixing zones so layers stay readable, optional "marbling" setting.
- ☐ Replay recorder: deterministic input log, so a run can be replayed exactly from seed and inputs;
  regression tests use recorded replays to catch physics changes.
- ☐ Fixed-point or cross-platform deterministic math check, so Daily seeds play the same on every platform.
- ☐ New themes (6 more, two unlockable by achievements) and animated backdrops.
- ☐ Pot shapes as cosmetics (wide, tall, curved, with a bump) that change physics a little.

Exit: replay tests pass bit-identically on Linux, macOS, Windows, Android. Tag `v1.4.0`.

## v1.5.0: Social without a server

Goal: sharing and friendly competition, with zero network code in the game.

- ☐ Share card: export a PNG of the final pot and score (system share sheet on mobile, file on desktop).
- ☐ Run codes: copy a short code that contains seed + inputs; friends paste it to replay your run or race it.
- ☐ Daily result text ("Liq2048 Daily 2026-12-01: 1024, 38 drops") copied to the clipboard in one tap.
- ☐ Local hot-seat versus mode: two players alternate drops in one pot, score by merges.
- ☐ Ghost races against your own best replay.
- ☐ Import/export of save data as a file (manual backup and moving between devices).

Exit: all sharing paths work on all five platforms without any permission beyond the share sheet. Tag `v1.5.0`.

## v1.6.0: Craft (input, desktop and engineering quality)

Goal: a solid, pleasant foundation.

- ☐ Gamepad support (SDL GameController): menus, aim, drop; Steam Deck verified checklist.
- ☐ Full keyboard navigation of all menus, remappable keys, mouse wheel to aim.
- ☐ Desktop polish: resizable window with integer scaling, borderless fullscreen, multi-monitor, high-DPI.
- ☐ Controller/keyboard glyph hints in the UI.
- ☐ Test coverage report in CI with a floor; fuzz tests for the JSON parser, save loader and OGG decoder;
  sanitizer builds (ASan/UBSan) in CI.
- ☐ Reproducible builds and checksums (`SHA256SUMS`) attached to every release; signed tags.
- ☐ Profiling overlay (frame time graph, particle count) behind a developer flag.
- ☐ Carried over from v1.1.0/v1.2.0: screen-reader labels (TalkBack, VoiceOver), large-text option,
  notch/safe-area handling, audio ducking for calls, aim snapping and preview-opacity options, lowered
  one-handed HUD.
- ☐ Source documentation pass: every header has a one-paragraph purpose comment; `docs/ARCHITECTURE.md` diagrams.

Exit: CI includes sanitizers and fuzz smoke tests, all green. Tag `v1.6.0`.

## v1.7.0: Stores and distribution

Goal: easy to install everywhere.

- ☐ Linux: AppImage and Flatpak (Flathub submission), `.deb`; metainfo and desktop files.
- ☐ Windows: signed installer or portable zip plus winget manifest; Microsoft Store optional.
- ☐ macOS: notarized build (needs an Apple developer certificate) and Homebrew cask.
- ☐ Android: Google Play listing (AAB, store assets, privacy statement "collects nothing"), plus F-Droid
  inclusion with reproducible build metadata.
- ☐ iOS: signed TestFlight/App Store build when a developer account is available; keep the unsigned IPA.
- ☐ itch.io page with all platforms; web demo build via Emscripten (WebAssembly) as a stretch goal.
- ☐ Store screenshots and short trailer generated from the headless `--screenshot` tool and branding scripts.
- ☐ Press kit page in `docs/` (logo, screenshots, description, facts).

Exit: at least Flathub, F-Droid or Play, and winget published. Tag `v1.7.0`.

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
- ☐ Local-network (LAN) versus mode over a direct connection, still with no servers and no accounts.
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
| Security and privacy | Dependencies updated, CodeQL clean, still zero network access and zero data collection. |
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
| v1.3.0 | Modes | ☐ |
| v1.4.0 | Liquid, second pass | ☐ |
| v1.5.0 | Social without a server | ☐ |
| v1.6.0 | Craft | ☐ |
| v1.7.0 | Stores and distribution | ☐ |
| v1.8.0 | Modding and community | ☐ |
| v2.0.0 | Next generation | ☐ |

Suggestions and priorities are welcome as GitHub issues and Discussions; the order of stations can change when
players ask for something else first.
