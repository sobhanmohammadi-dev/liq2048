# Localization

Shipped languages (8): **English** (`en`, fallback), **Persian** (`fa`), **Spanish** (`es`), **French** (`fr`),
**German** (`de`), **Turkish** (`tr`), **Portuguese** (`pt`) and **Arabic** (`ar`). Persian and Arabic are
right-to-left. The default is the system language if supported, otherwise English. The player picks a language in
Settings (General page); the in-game `EN`/`xx` button toggles between English and the last other language used.

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

1. Copy `en.json` to `assets/lang/<code>.json` and translate every value. Keep strings about as short as the
   English ones (the UI is 270 px wide; setting labels must fit about 16 characters).
2. Add the language to `kLangs`/`langInfo()`/`langAt()` in `src/i18n/Loc.hpp` (code, picker name, `rtl`,
   `arabicScript`) and raise `kLangCount`.
3. **Latin scripts** use the built-in pixel font, which is all-caps: accented letters are folded to their base letters
   (`PixelFont::ascii`: é→E, ñ→N, ç→C, ğ→G, ş→S, ı/İ→I, ß→SS; German ä/ö/ü→AE/OE/UE). Write the JSON with correct
   accents, the folding happens when drawing. Other scripts (Cyrillic, CJK, Devanagari) need a TTF and a new
   face in `Gfx`; this is planned for a later station.
4. **Arabic-script** languages (`arabicScript = true`) are drawn with `assets/fonts/fa.ttf` (Vazirmatn).
5. Run `ctest`: `all_languages_complete` fails if a key is missing, empty, has a stray extra key, or loses `{0}`.

## Right-to-left support

- `Ui::mx(x, w)` mirrors rectangles; `Ui::start()` returns the leading alignment. Scenes use them for lists, rows
  and HUD slots (next preview and pause button swap sides).
- Text is drawn with SDL_ttf using direction `RTL` and script `Arab` when SDL_ttf ≥ 2.20 has HarfBuzz, giving
  correct letter joining.
- SDL_ttf does not implement bidi, so `Gfx` pre-reverses embedded ASCII runs (digits, Latin words) in Persian
  strings. Keep such runs short and standalone (`"First 128"`) rather than mixing punctuation into them.

## Fonts for Persian and Arabic

`assets/fonts/fa.ttf` is Vazirmatn and ships in the repository. Joining of letters, lam-alef ligatures and the
right-to-left order are done by the game (`ArabicShaper`), not by SDL_ttf, which keeps all platforms identical. If the file is missing, the game tries
common system fonts (Tahoma, DejaVu Sans, Noto Naskh Arabic, Droid Arabic). When none is found, the language
button is disabled and the game stays in English; it never shows broken text.
