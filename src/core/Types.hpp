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
    auto m = [t](Uint8 x, Uint8 y) { return Uint8(float(x) + (float(y) - float(x)) * t + 0.5f); };
    return Color{m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a)};
}

inline Color withAlpha(Color c, Uint8 a) {
    c.a = a;
    return c;
}

constexpr uint32_t pack(Color c) { return (uint32_t(c.a) << 24) | (uint32_t(c.r) << 16) | (uint32_t(c.g) << 8) | c.b; }
constexpr Color unpack(uint32_t p) {
    return Color{Uint8((p >> 16) & 255), Uint8((p >> 8) & 255), Uint8(p & 255), Uint8(p >> 24)};
}

inline int luminance(Color c) { return (c.r * 299 + c.g * 587 + c.b * 114) / 1000; }

enum class ModeId { Classic = 0, Timed, Target, Daily, Zen, Rain, Tide, Weekly, Puzzle };
constexpr int kModeCount = 9;

enum class Align { Left, Center, Right };

// What the blob rasterizer needs to draw one droplet: an ellipse (axes ra along the
// direction cs/sn, rb across it) and a level that selects colour and label.
struct BlobSprite {
    float x = 0, y = 0;
    float ra = 8, rb = 8;
    float cs = 1, sn = 0;
    int level = 0;
};

}  // namespace liq
