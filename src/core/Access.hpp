#pragma once
// Accessibility variants of a theme: colour-vision-safe droplet palettes and a high-contrast UI.
// The droplet palettes were found by searching for 12 colours (ordered by lightness) whose minimum
// pairwise CIE Lab distance stays large under protanopia, deuteranopia (red/green safe) or tritanopia
// (blue/yellow safe) simulation; the standard palette scores about 3 to 8 there, these about 22.
#include "core/Themes.hpp"

namespace liq {

enum class ColorVision { Standard = 0, RedGreenSafe = 1, BlueYellowSafe = 2 };
inline constexpr int kColorVisionCount = 3;
inline const char* colorVisionKey(int m) {
    return m == 1 ? "access.cvd.rg" : m == 2 ? "access.cvd.by" : "access.cvd.std";
}

inline Theme accessTheme(const Theme& base, int colorVision, bool highContrast) {
    Theme t = base;
    static const Color rg[12] = {rgb(0x3291ff), rgb(0x329eb0), rgb(0x9d9432), rgb(0x40a479), rgb(0x32bbff), rgb(0xd1bb32),
                                 rgb(0x38d8c0), rgb(0xa5cf80), rgb(0x43efff), rgb(0xa7ffc7), rgb(0xe8fb32), rgb(0xd2ff84)};
    static const Color by[12] = {rgb(0xff5546), rgb(0xa98f32), rgb(0xad74ff), rgb(0xdb7a4b), rgb(0x748dff), rgb(0xff33f6),
                                 rgb(0x81aa37), rgb(0x32c032), rgb(0xfe9cf8), rgb(0xe0e857), rgb(0xabff36), rgb(0x68ffff)};
    if (colorVision == 1 || colorVision == 2)
        for (int i = 0; i < 12; ++i) t.blob[i] = colorVision == 1 ? rg[i] : by[i];
    if (highContrast) {
        t.ink = rgb(0x000000);
        t.text = rgb(0xffffff);
        t.dim = mix(base.dim, rgb(0xffffff), 0.55f);
        t.panel = mix(base.panel, rgb(0x000000), 0.45f);
        t.panelLo = mix(base.panelLo, rgb(0x000000), 0.5f);
        t.panelHi = mix(base.panelHi, rgb(0xffffff), 0.35f);
        t.tank = mix(base.tank, rgb(0x000000), 0.6f);
        t.label = rgb(0x1a0f33);
        t.accent = mix(base.accent, rgb(0xffffff), 0.25f);
        t.danger = mix(base.danger, rgb(0xffffff), 0.2f);
    }
    return t;
}

}  // namespace liq
