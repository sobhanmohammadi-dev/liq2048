#pragma once
// Small shared value types used across modules.
#include <SDL.h>
#include <cstdint>

namespace liq {

struct Color {
    Uint8 r, g, b, a;
};

constexpr Color rgb(uint32_t hex, Uint8 a = 255) {
    return Color{Uint8((hex >> 16) & 255), Uint8((hex >> 8) & 255), Uint8(hex & 255), a};
}

inline Color mix(Color a, Color b, float t) {
    auto m = [t](Uint8 x, Uint8 y) { return Uint8(x + (y - x) * t); };
    return Color{m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a)};
}

inline Color withAlpha(Color c, Uint8 a) {
    c.a = a;
    return c;
}

inline int luminance(Color c) { return (c.r * 299 + c.g * 587 + c.b * 114) / 1000; }

enum class ModeId { Classic = 0, Timed, Target, Daily, Zen };
constexpr int kModeCount = 5;

enum class Align { Left, Center, Right };

}  // namespace liq
