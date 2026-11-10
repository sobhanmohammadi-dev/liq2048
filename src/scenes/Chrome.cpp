// Scene chrome shared by several scenes: wordmark, tank frame, overflow line and plain droplet sprites.
#include <cctype>
#include <cmath>
#include <string>

#include "core/Access.hpp"
#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "game/Liquid.hpp"
#include "game/World.hpp"
#include "scenes/Scenes.hpp"

namespace liq {

BlobSprite plainSprite(float x, float y, int level, float wobble, float phase) {
    BlobSprite s;
    s.x = x;
    s.y = y;
    s.level = level;
    const float r = World::radiusFor(level);
    if (wobble > 0.f) {
        const float th = phase;  // orientation of the stretch axis
        s.cs = std::cos(th);
        s.sn = std::sin(th);
        s.ra = r * (1.f + wobble);
        s.rb = r / (1.f + wobble);
    } else {
        s.ra = s.rb = r;
    }
    return s;
}

void drawWordmark(Ctx& c, float cx, float cy, int scale) {
    Gfx& g = c.gfx;
    const Theme& t = *c.theme;
    const std::string w = "LIQ2048";
    // Chunky extrusion: stack darker copies below, outline the lowest and the top copy.
    g.number(w, cx, cy + float(scale), scale, t.accentDark, Align::Center, TextFx::Outline, t.ink);
    for (int i = scale - 1; i >= 1; --i) g.number(w, cx, cy + float(i), scale, t.accentDark, Align::Center);
    g.number(w, cx, cy, scale, t.accent, Align::Center, TextFx::Outline, t.ink);
}

void drawTankFrame(Ctx& c) {
    Gfx& g = c.gfx;
    const Theme& t = *c.theme;
    const int L = int(cfg::kFieldL), R = int(cfg::kFieldR), top = cfg::kTankTop, fl = int(cfg::kFloor), bot = fl + 10;

    // Floor: ink edge, light top row, brick joints, dark bottom row.
    const int fx = L - 6, fw = R - L + 12;
    g.rect(float(fx), float(fl), float(fw), 10.f, t.frame);
    g.rect(float(fx), float(fl), float(fw), 1.f, t.ink);
    g.rect(float(fx + 1), float(fl + 1), float(fw - 2), 1.f, t.frameHi);
    g.rect(float(fx + 1), float(fl + 5), float(fw - 2), 1.f, t.frameLo);
    g.rect(float(fx + 1), float(fl + 8), float(fw - 2), 1.f, t.frameLo);
    g.rect(float(fx), float(fl + 9), float(fw), 1.f, t.ink);
    for (int x = fx + 10; x < fx + fw - 2; x += 24) g.rect(float(x), float(fl + 2), 1.f, 3.f, t.frameLo);
    for (int x = fx + 22; x < fx + fw - 2; x += 24) g.rect(float(x), float(fl + 6), 1.f, 2.f, t.frameLo);

    // Walls: ink | light | frame frame | dark | ink.
    auto wall = [&](int x) {
        g.rect(float(x), float(top), 1.f, float(bot - top), t.ink);
        g.rect(float(x + 1), float(top), 1.f, float(bot - top - 1), t.frameHi);
        g.rect(float(x + 2), float(top), 2.f, float(bot - top - 1), t.frame);
        g.rect(float(x + 4), float(top), 1.f, float(bot - top - 1), t.frameLo);
        g.rect(float(x + 5), float(top), 1.f, float(bot - top), t.ink);
    };
    wall(L - 6);
    wall(R);
    for (int y = top + 16; y < fl - 6; y += 34) {  // rivets
        g.rect(float(L - 4), float(y), 2.f, 2.f, t.frameLo);
        g.rect(float(R + 2), float(y), 2.f, 2.f, t.frameLo);
    }
    for (int x : {L - 8, R - 2}) {  // wall caps
        g.rect(float(x - 1), float(top - 5), 12.f, 6.f, t.ink);
        g.rect(float(x), float(top - 4), 10.f, 4.f, t.frame);
        g.rect(float(x), float(top - 4), 10.f, 1.f, t.frameHi);
    }
}

void drawDeadLine(Ctx& c, bool warn, float lineY) {
    Gfx& g = c.gfx;
    const Theme& t = *c.theme;
    const int y = int(lineY);
    const int shift = warn ? int(c.time * 24.f) % 8 : 0;  // marching dashes while in danger
    const Color col = warn ? ((int(c.time * 8.f) & 1) ? t.danger : mix(t.danger, t.ink, 0.35f)) : mix(t.dim, t.tank, 0.35f);
    for (int x = int(cfg::kFieldL) - shift; x < int(cfg::kFieldR); x += 8) {
        const int x0 = std::max(x, int(cfg::kFieldL)), x1 = std::min(x + 5, int(cfg::kFieldR));
        if (x1 > x0) g.rect(float(x0), float(y), float(x1 - x0), 1.f, col);
    }
}

}  // namespace liq
