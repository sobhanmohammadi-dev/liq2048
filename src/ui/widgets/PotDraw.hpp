#pragma once
// Draws the obstacles of an obstacle pot (game/Pot.hpp) in the pot frame's colours.
#include <vector>

#include "game/World.hpp"
#include "gfx/Gfx.hpp"

namespace liq {

inline void drawObstacles(Gfx& g, const Theme& t, const std::vector<World::Disc>& obs, float ox, float oy) {
    for (const auto& o : obs) g.disc(ox + o.x, oy + o.y, int(o.r) + 1, t.ink);
    for (const auto& o : obs) g.disc(ox + o.x, oy + o.y, int(o.r), t.frame);
    for (const auto& o : obs) g.disc(ox + o.x - 1.f, oy + o.y - 1.f, std::max(1, int(o.r) - 3), t.frameHi);
}

}  // namespace liq
