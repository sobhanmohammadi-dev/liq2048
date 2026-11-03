# Accessibility

Liq2048 aims to be playable by as many people as possible. Everything below is in **Settings**
(General, Game feel and Access pages) and is saved.

| Need | Option | Notes |
|------|--------|-------|
| Colour-vision deficiency | Access → Colour vision: Standard / Red-Green / Blue-Yellow | Alternative 12-colour droplet palettes ordered by lightness. A search maximised the minimum CIE Lab distance between any two levels under protanopia, deuteranopia (Red-Green) or tritanopia (Blue-Yellow) simulation: about 22, against 3 to 8 for the standard palette. Every droplet also carries its number, so level never depends on colour alone. A live swatch preview is shown in the settings. |
| Low vision | Access → High contrast | Pure white text, black outlines, darker panels and tank, brighter dim text, accent and danger colours. |
| Motor / touch precision | Access → Big touch areas; invisible touch margin around every button on phones (3 px, 6 px with the option) | Aiming works anywhere on the board; drop on release. |
| One-handed or left-handed play | Game feel → Left-hand HUD | Mirrors the in-game HUD (pause, language, next preview, undo). |
| Motion sensitivity | Game feel → Reduced motion, Calm mode, Screen shake, Slow motion | Reduced motion also removes scene transitions, particles, rings, drifting backdrop and bubbles. |
| Vibration sensitivity | Game feel → Vibration (phones) | |
| Keyboard / gamepad-less play | Arrow keys or A/D, Space/Down to drop, U/Z undo, Esc pause | Full keyboard focus in menus. |
| Language | Settings → Language (8 languages, two right-to-left) | See [LOCALIZATION.md](LOCALIZATION.md). |
| Hearing | Every sound has a visual counterpart (merge rings, chain banner, shake); music and sound volumes are separate | |

## Known gaps (tracked in the roadmap)

- Screen-reader labels (TalkBack, VoiceOver) are not implemented yet.
- No large-text option yet: the UI uses a fixed 270x480 pixel grid.
- Cyrillic, CJK and Devanagari scripts are not supported yet (they need extra fonts).
- Gamepad support is planned for a later station.

## Checklist for new features

1. Does it work with every colour-vision palette? Never encode information in colour alone.
2. Does it respect Reduced motion and Calm mode?
3. Are touch targets at least 24 px with the margin, and reachable one-handed?
4. Are all strings in the language files, short enough, and complete in all 8 languages?
