# Localization

Shipped languages: **English** (`en`, fallback) and **Persian** (`fa`, right-to-left).
The default is the system language if supported, otherwise English. The player's choice is saved.

## String tables

`assets/lang/<code>.json` is a flat object of `"key": "value"` pairs.

```json
{
  "menu.play": "Play",
  "menu.streak": "Daily streak: {0}"
}
```

- `{0}` is replaced by `Loc::fmt(key, arg)`.
- `\n`, `\t`, `\"` and `\\` escapes are supported. Only string values are read (see `src/core/Json.hpp`).
- Missing keys fall back to English, and then to the key itself, so a missing translation is never blank.
- **Numbers and scores always use Western digits** (0–9), also inside Persian text.

## Adding a language

1. Copy `en.json` to `assets/lang/<code>.json` and translate every value.
2. Provide a font that covers the script (see [ASSETS.md](ASSETS.md)).
3. Register the code: `Ctx::setLanguage` and the language toggle in `src/scenes/Common.cpp`
   (today the toggle switches EN ⇄ FA; extend it to cycle or pick from a list).
4. If the language is right-to-left, extend `Loc::rtl()`.
5. Run `ctest`: the localization test fails if a key is missing or if a language has an unknown extra key.

## Right-to-left support

- `Ui::mx(x, w)` mirrors rectangles; `Ui::start()` returns the leading alignment. Scenes use them for lists, rows
  and HUD slots (next preview and pause button swap sides).
- Text is drawn with SDL_ttf using direction `RTL` and script `Arab` when SDL_ttf ≥ 2.20 has HarfBuzz, giving
  correct letter joining.
- SDL_ttf does not implement bidi, so `Gfx` pre-reverses embedded ASCII runs (digits, Latin words) in Persian
  strings. Keep such runs short and standalone (`"First 128"`) rather than mixing punctuation into them.

## Fonts for Persian

`assets/fonts/fa.ttf` is preferred (Vazirmatn is a good open-licensed choice). If it is missing, the game tries
common system fonts (Tahoma, DejaVu Sans, Noto Naskh Arabic, Droid Arabic). When none is found, the language
button is disabled and the game stays in English; it never shows broken text.
