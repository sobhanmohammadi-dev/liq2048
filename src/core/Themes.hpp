#pragma once
// Colour themes. Every colour in the game comes from here, so a theme is a complete
// palette. Themes are cosmetic, unlocked through play (see Achievements.hpp), never bought.
#include "core/Types.hpp"

namespace liq {

struct Theme {
    const char* key;
    Color sky[5];                 // background gradient, top -> bottom (dithered)
    Color star, star2;
    Color ink;                    // darkest outline colour
    Color panel, panelHi, panelLo;
    Color tank, tankDot;
    Color frame, frameHi, frameLo;
    Color text, dim, accent, accentDark, danger;
    Color blob[12];               // level 0..11 (bright fills; labels use `label` on top)
    Color label;                  // flat number colour drawn on blobs
};

inline constexpr int kThemeCount = 4;

inline const Theme& themeAt(int i) {
    static const Theme t[kThemeCount] = {
        // Night (default)
        {"theme.default",
         {rgb(0x2a2160), rgb(0x231b52), rgb(0x1c1643), rgb(0x151136), rgb(0x0f0c28)},
         rgb(0xcfc8ff), rgb(0xffe27a), rgb(0x0b0920),
         rgb(0x2c2566), rgb(0x4337a0), rgb(0x1d1848),
         rgb(0x0f0c2a), rgb(0x1b1742),
         rgb(0xf0b429), rgb(0xffe08a), rgb(0xb97a16),
         rgb(0xf6f2ff), rgb(0x8f88cf), rgb(0xffc83d), rgb(0xb97a16), rgb(0xff5470),
         {rgb(0x66e8ff), rgb(0x4fb8ff), rgb(0xb4a6ff), rgb(0xf0aefc), rgb(0xff78ee), rgb(0xff8ca6), rgb(0xffb04a), rgb(0xffee3a), rgb(0xdff566), rgb(0x78f0a8), rgb(0xf2f4ff), rgb(0xffd24a)}, rgb(0x5b3a9c)},
        // Steel
        {"theme.mono",
         {rgb(0x2b2d33), rgb(0x24262b), rgb(0x1d1f23), rgb(0x16171a), rgb(0x101113)},
         rgb(0xd0d4dc), rgb(0xffffff), rgb(0x07080a),
         rgb(0x33363e), rgb(0x4d525e), rgb(0x22242a),
         rgb(0x0c0d0f), rgb(0x1a1c20),
         rgb(0xcfd4de), rgb(0xffffff), rgb(0x8a909e),
         rgb(0xf2f4f8), rgb(0x8a909e), rgb(0xffd23d), rgb(0xb8901a), rgb(0xff5a5a),
         {rgb(0x7d8fa8), rgb(0x93a6c0), rgb(0xa9bcd6), rgb(0xbfd0e6), rgb(0xd2e0f0), rgb(0xe4eef8), rgb(0xf4f8fc), rgb(0xfff1b8), rgb(0xffe27a), rgb(0xffd24a), rgb(0xffbf33), rgb(0xffa800)}, rgb(0x23283a)},
        // Sunset
        {"theme.sunset",
         {rgb(0x4a1d52), rgb(0x3d1748), rgb(0x30103c), rgb(0x240b30), rgb(0x190825)},
         rgb(0xffd6a5), rgb(0xfff4e0), rgb(0x12051c),
         rgb(0x4a2058), rgb(0x6b3278), rgb(0x301240),
         rgb(0x160722), rgb(0x25103a),
         rgb(0xff9a5a), rgb(0xffc896), rgb(0xc2562b),
         rgb(0xfff4e8), rgb(0xb07aa0), rgb(0xff9a5a), rgb(0xc2562b), rgb(0xff4f6a),
         {rgb(0xffe0b0), rgb(0xffb86b), rgb(0xff9a6a), rgb(0xff8a8a), rgb(0xff7ab0), rgb(0xf08ae0), rgb(0xc8a0ff), rgb(0x9ab4ff), rgb(0x7ad4ff), rgb(0x8ff0c0), rgb(0xfff2a0), rgb(0xffc85a)}, rgb(0x4a1d52)},
        // Forest
        {"theme.forest",
         {rgb(0x17402f), rgb(0x133628), rgb(0x0f2c21), rgb(0x0b231a), rgb(0x081a13)},
         rgb(0xb8f2c4), rgb(0xf4ffd0), rgb(0x04120c),
         rgb(0x1f4d3a), rgb(0x2f6e52), rgb(0x123325),
         rgb(0x08170f), rgb(0x11291d),
         rgb(0x7dd87a), rgb(0xc6f5a8), rgb(0x3f9a4c),
         rgb(0xf0fff0), rgb(0x6fa88a), rgb(0x9be564), rgb(0x3f9a4c), rgb(0xff6a5a),
         {rgb(0xd6f5a0), rgb(0xa4e35a), rgb(0x7ee08a), rgb(0x5fe0b8), rgb(0x5cd0e8), rgb(0x7ab8ff), rgb(0xa8a0ff), rgb(0xd8a0ff), rgb(0xff9ad0), rgb(0xffb07a), rgb(0xffe07a), rgb(0xfff7c0)}, rgb(0x1a3d2c)},
    };
    return t[i < 0 || i >= kThemeCount ? 0 : i];
}

}  // namespace liq
