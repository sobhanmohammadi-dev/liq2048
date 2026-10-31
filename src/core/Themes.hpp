#pragma once
// Cosmetic colour themes. Unlocked through play (see Achievements.hpp), never bought.
#include "core/Types.hpp"

namespace liq {

struct Theme {
    const char* key;
    Color bg, panel, frame, text, dim, accent, danger;
    Color blob[12];
};

inline constexpr int kThemeCount = 4;

inline const Theme& themeAt(int i) {
    static const Theme t[kThemeCount] = {
        {"theme.default", rgb(0x14122b), rgb(0x241f4a), rgb(0xffc83d), rgb(0xffffff), rgb(0x7f7ab0), rgb(0xffc83d), rgb(0xff5c7a),
         {rgb(0x5ee6ff), rgb(0x3fa9ff), rgb(0xa89cff), rgb(0xe0a8ff), rgb(0xff7ad9), rgb(0xff5c7a),
          rgb(0xffc83d), rgb(0xffe66b), rgb(0xb8f26b), rgb(0x4fe0a0), rgb(0xf2f2f2), rgb(0xffd700)}},
        {"theme.mono", rgb(0x101010), rgb(0x262626), rgb(0xdddddd), rgb(0xffffff), rgb(0x777777), rgb(0xffd700), rgb(0xff5555),
         {rgb(0x6e6e6e), rgb(0x808080), rgb(0x929292), rgb(0xa4a4a4), rgb(0xb6b6b6), rgb(0xc8c8c8),
          rgb(0xd8d8d8), rgb(0xe4e4e4), rgb(0xeeeeee), rgb(0xf6f6f6), rgb(0xffffff), rgb(0xffd700)}},
        {"theme.sunset", rgb(0x2b1220), rgb(0x45203a), rgb(0xff9a5a), rgb(0xfff4e8), rgb(0xa8708a), rgb(0xff9a5a), rgb(0xff4f6a),
         {rgb(0xffd6a5), rgb(0xffb677), rgb(0xff9a5a), rgb(0xff7a5a), rgb(0xff5c6a), rgb(0xe64a8a),
          rgb(0xc23fa0), rgb(0x9a3fb5), rgb(0x7a4fc9), rgb(0xffc83d), rgb(0xfff1a8), rgb(0xffffff)}},
        {"theme.forest", rgb(0x0f2018), rgb(0x1d3a2c), rgb(0x7dd87a), rgb(0xf0fff0), rgb(0x5f8f78), rgb(0x7dd87a), rgb(0xff6a5a),
         {rgb(0xc9f2a0), rgb(0xa4e87a), rgb(0x7dd87a), rgb(0x5cc98a), rgb(0x45b8a0), rgb(0x3aa6b8),
          rgb(0x4a8fd0), rgb(0x6a7fe0), rgb(0xffe66b), rgb(0xffc83d), rgb(0xffffff), rgb(0xffd700)}},
    };
    return t[i < 0 || i >= kThemeCount ? 0 : i];
}

}  // namespace liq
