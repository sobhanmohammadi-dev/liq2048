# Architecture

Liq2048 is a small, layered C++17 codebase. `liq_core` (a static library) holds everything except
`main.cpp`, so tests and tools link the same code the game runs.

```
                      main.cpp  (CLI parsing)
                          │
                       core/App ── main loop, SDL events, scene fades, headless capture
                          │
                    scenes/Ctx  ── shared context, passed by reference (no globals)
        ┌───────────┬─────┴──────┬───────────┬────────────┐
       Gfx        Ui / Input    Loc         Save        Audio
   (rendering)  (widgets)   (strings)   (progress)    (sound)
                          │
      scenes: Menu · ModeSelect · Progress · Settings · GameScene
                          │
              game/Mode (rules)      game/World (physics)
```

## Modules

| Module | Path | Responsibility |
|--------|------|----------------|
| App | `src/core/App.*` | SDL init, window, event pump, fixed/variable timestep, frame limiter (VSync / cap / unlimited), blinds scene transitions, `--screenshot` capture |
| Config | `src/core/Config.hpp` | Every tunable: screen layout, physics, gameplay timings |
| Platform | `src/core/Platform.*` | Asset lookup (`exe/assets`, `./assets`, Android APK), pref dir, system language, UTC day |
| Gfx | `src/gfx/Gfx.*` | Renderer at a fixed 360×640 logical size, flat primitives, 5×7 bitmap font, optional SDL_ttf text cache |
| Ui / Input | `src/ui/*` | Immediate-mode buttons/panels/paragraphs, RTL mirroring, keyboard focus |
| World | `src/game/World.*` | Blob physics and merge/chain detection |
| Mode | `src/game/Mode.*` | Strategy objects for the five modes |
| Scenes | `src/scenes/*` | `Scene::frame(Ctx&, dt)` updates and draws one screen |
| Loc | `src/i18n/Loc.*` | JSON string tables with English fallback |
| Save | `src/save/*` | `key=value` progress file, achievements, streaks |
| Audio | `src/audio/Audio.*` | Loads OGG files from `assets/sounds` and `assets/music` (stb_vorbis), mixes SFX voices and cross-fades music loops in an SDL callback; no-op without a device or files |

## Design decisions

- **No global state.** `Ctx` bundles `Gfx`, `Loc`, `Save`, `Audio`, `Input` and `Ui` and is passed to scenes.
- **Immediate-mode UI.** A scene draws and handles input in one `frame()` call. Buttons are matched by call
  order inside a frame; keyboard focus walks the focusable ones. This keeps scenes short and easy to follow.
- **Resolution independence.** All code works in 360×640 logical pixels; SDL scales to the window with
  nearest-neighbour filtering, so pixel art stays crisp on any screen. Mouse coordinates arrive already mapped.
- **Playable without assets.** All visuals are code-drawn and a built-in bitmap font exists. Optional TTF fonts,
  sounds and music are discovered at runtime.
- **Strategy for modes.** `GameScene` runs the shared loop (aim, drop, simulate, score). A `Mode` supplies the
  seed, drop limit, gravity scale, win/lose evaluation and HUD text. Adding a mode touches `Mode.cpp` plus
  strings; see [GAMEPLAY.md](GAMEPLAY.md).
- **Deterministic daily challenge.** The drop sequence comes from a seeded `mt19937` using raw output (not
  `std::*_distribution`, which differs between standard libraries), so everyone gets the same sequence for a UTC day.

## Physics

`World` uses **position-based dynamics** on circles:

1. For each of 4 substeps per 60 Hz frame: integrate gravity and velocity.
2. Run 8 solver iterations: clamp to the container, then push every overlapping pair apart. Displacement is split
   by inverse mass (`1/r²`), so big blobs move less.
3. Derive velocity from the position change, apply damping (and extra floor friction), clamp speed.
4. **Merge pass:** two same-level blobs whose distance ≤ `r₁ + r₂ + 1` fuse into one blob of the next level at the
   mass-weighted midpoint. The new blob gets a visual *pop* and a short age grace period.

*Chains:* merges within `kChainWindow` (0.7 s) of the previous one increment a chain counter; score is
`value × chain`.

*Overflow:* a blob older than 1.2 s, slower than 80 px/s, whose top is above the dead line accumulates
overflow time. After 2 s the run is lost (Zen instead pops those blobs).

The "liquid" look is deliberately a cheap illusion: impact speed produces a squash on the ellipse, merges produce
a pop, and damping makes piles settle softly. PBD is unconditionally stable for stacks, which matters more here
than true fluid behaviour.

## Rendering

Blobs are drawn as filled ellipses from 1-pixel-high runs (`Gfx::ellipse`) with a darker outline, a pixel highlight
and a bitmap-font number. Text is drawn by the bitmap font unless a TTF is loaded for the active language. Numbers
always use the bitmap font so digits stay Western and crisp.

SDL_ttf has no bidi pass, so for Persian `Gfx` pre-reverses embedded ASCII runs (digits/Latin) to keep them
readable inside right-to-left strings (`rtlFix` in `Gfx.cpp`).

## Testing

`tests/test_main.cpp` is a dependency-free test runner covering physics (merge, chain, bounds, overflow), RNG
determinism, every mode's win/lose logic, JSON parsing, localization key parity and save/streak logic.
It runs in CI on every push.


## Liquid simulation and rendering (v1.1)

Everything is drawn on a fixed 270×480 pixel grid scaled by an integer factor with nearest filtering.

- **`World`** is a particle fluid. Each drop is a cluster of particles (count fixed per level, so volume is
  constant). Double-density relaxation keeps the liquid incompressible, viscosity makes it thick like paint,
  and same-drop cohesion keeps each colour in its own body. A drop therefore falls as a droplet, splashes,
  spreads over the floor and levels out in the pot. Two touching drops of the same level merge: their particles
  are joined, trimmed to the next level's volume and given a small splash impulse. Neighbour search uses a
  spatial hash built once per fixed step. Numbers are anchored at the deepest part of each body (`World::anchors`).
- **`BlobRaster::drawLiquid`** rasterizes the particles with table kernels: summed field above an iso level is
  liquid, the two strongest colours at a pixel are mixed with an 8×8 Bayer dither (pixel-art blending), and
  only the outer silhouette gets a 1px outline. No glow or lighting. `drawField` (ellipse metaballs) is used for
  the held drop, previews, menus and branding.
- **Frame rate:** the setting selects VSync, a sleep+spin cap or unlimited (`Save::fpsLimit`); particles are
  interpolated between 60 Hz physics steps.
- Tunables live in `src/core/Config.hpp` (`cfg::kStiff`, `kViscSigma`, `kCohesion`, ...).
