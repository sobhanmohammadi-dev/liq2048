# Gameplay and design

## Core loop

Press and drag to position the blob above the container, release to drop it. Two blobs with the same number
merge into the next power of two (1+1→2 … 1024+1024→2048). 2048 blobs are the cap and do not merge further.
Do not let blobs rest above the dashed line for more than two seconds.

**Controls:** mouse/touch drag and release · ←/→ or A/D aim · Space/↓ drop · ↑/↓ + Enter/Space in menus ·
Esc pause/back · F11 fullscreen.

The dashed guide and ghost circle show where the blob will land. A short cooldown (0.45 s) after each drop
prevents accidental double-drops; there are no other waits.

## Scoring

`score += value_of_new_blob × chain`. A chain is a merge within 0.7 s of the previous merge, so one clever drop
that cascades scores much more than the same merges done separately.

## Modes

| Mode | Rules | Ends when |
|------|-------|-----------|
| **Classic** | Endless | Overflow |
| **Time Attack** | 90 s | Time runs out (or overflow) |
| **Target** | Reach 256 within 140 drops. Win → next run asks for 512 (260 drops), then 1024 (520 drops) | Tile reached, drops run out, or overflow |
| **Daily Challenge** | Same seed for everyone each UTC day, 60 drops | Drops used up and board settled, or overflow |
| **Zen** | Gravity ×0.35, no fail state. Blobs stuck above the line pop harmlessly | You quit |

Random drops are weighted: 1 (40 %), 2 (30 %), 4 (20 %), 8 (10 %).

## Progression (no dark patterns)

- **Best scores** per mode, plus today's best for the Daily Challenge.
- **Achievements:** first 128 / 256 / 512 / 1024 / 2048, chain ×3, chain ×5, score 1000, score 5000.
- **Themes** (colours only, earned never bought): Default, Mono (first 128), Sunset (chain ×5), Forest (first 512).
- **Daily streak:** counts consecutive days played. It resets quietly; there are no penalties, notifications or
  expiring rewards.
- No ads, in-app purchases, paid power-ups, energy timers, loot boxes or tracking. The game never touches the network.

## Adding a mode

1. Add the id to `ModeId` (`src/core/Types.hpp`) and bump `kModeCount`.
2. Subclass `Mode` in `src/game/Mode.cpp` (`nameKey`, `evaluate`, optional `seed`, `dropLimit`,
   `gravityScale`, `failable`, `status`, `progress`) and register it in `makeMode`.
3. Add `mode.<name>`, `mode.<name>.desc` (and any HUD strings) to every `assets/lang/*.json`.
4. Add the new entry to the arrays in `ModeSelectScene` / `ProgressScene` (`src/scenes/MenuScenes.cpp`).
5. Add a test in `tests/test_main.cpp`.

## Adding a theme

Add an entry to `themeAt` in `src/core/Themes.hpp` (increase `kThemeCount`), an unlock achievement in
`themeUnlockAch` (`src/save/Achievements.hpp`) and a `theme.<name>` string in each language file.

## Tuning feel

All physics and timing constants live in `src/core/Config.hpp`: gravity, damping, solver iterations, blob radius
growth, chain window, drop cooldown. Change a value, rebuild, and play: no other code needs touching.

## Undo, feel and comfort options (1.1.0)

- **Undo** (Classic and Zen): the arrow button on the left of the HUD, or `U` / `Z`, takes back your last drop.
  You start each run with 1 charge and earn another for every chain of 3 or more (up to 3).
- **Settings → More** has: aim guide, vibration (phones), screen shake, slow motion and **Calm mode**
  (no shake, slow motion, blinking or flashes; fewer particles).
- The game pauses by itself when the window loses focus or the app goes to the background.
