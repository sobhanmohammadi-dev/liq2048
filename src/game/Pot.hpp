#pragma once
// Obstacle pots (v1.9.10): the pot can hold fixed obstacles that shape how the liquid piles and flows. Every pot is
// built from discs, so one cheap collision rule covers walls, ramps and pegs. The shapes scale with the pot, so they
// also fit the big container. The pot is part of a run (replay codes keep it) and it is only offered where the
// rules allow a free choice.
#include <cmath>

#include "game/World.hpp"

namespace liq {

enum Pot { kPotClassic, kPotPegs, kPotDivider, kPotFunnel, kPotLedges, kPotCount };

inline const char* potKey(int p) {
    static const char* k[kPotCount] = {"pot.classic", "pot.pegs", "pot.divider", "pot.funnel", "pot.ledges"};
    return k[p < 0 || p >= kPotCount ? 0 : p];
}

// Fills w.obstacles for pot `p` inside the world's current field (call after the field is set up).
inline void applyPot(World& w, int p) {
    w.obstacles.clear();
    if (p <= kPotClassic || p >= kPotCount) return;
    const float L = w.fieldL, W = w.fieldR - w.fieldL, F = w.floorY;
    const float H = std::min(F, 330.f);  // pot height the shapes are laid out for
    auto at = [&](float u) { return L + W * u; };
    auto up = [&](float v) { return F - v * H; };  // v: 0 = floor .. 1 = a full pot
    auto line = [&](float x0, float y0, float x1, float y1, float r) {  // a chain of overlapping discs
        const float len = std::hypot(x1 - x0, y1 - y0);
        const int n = std::max(1, int(len / (r * 0.9f)));
        for (int i = 0; i <= n; ++i) {
            const float u = float(i) / float(n);
            w.obstacles.push_back({x0 + (x1 - x0) * u, y0 + (y1 - y0) * u, r});
        }
    };
    switch (p) {
        case kPotPegs:  // three staggered rows of pegs: every drop is deflected
            for (const float u : {0.28f, 0.72f}) w.obstacles.push_back({at(u), up(0.20f), 7.f});
            for (const float u : {0.12f, 0.50f, 0.88f}) w.obstacles.push_back({at(u), up(0.38f), 7.f});
            for (const float u : {0.28f, 0.72f}) w.obstacles.push_back({at(u), up(0.56f), 7.f});
            break;
        case kPotDivider:  // a wall in the middle splits the bottom into two basins
            line(at(0.5f), F + 2.f, at(0.5f), up(0.42f), 4.5f);  // starts below the floor so no gap is left under it
            break;
        case kPotFunnel:  // two ramps leave a gap: everything above must flow through it
            line(L + 2.f, up(0.62f), at(0.5f) - 30.f, up(0.46f), 4.5f);
            line(L + W - 2.f, up(0.62f), at(0.5f) + 30.f, up(0.46f), 4.5f);
            break;
        case kPotLedges:  // staircase ledges from both walls
            line(L + 2.f, up(0.22f), at(0.52f), up(0.22f), 4.5f);
            line(L + W - 2.f, up(0.46f), at(0.48f), up(0.46f), 4.5f);
            break;
        default: break;
    }
}

}  // namespace liq
