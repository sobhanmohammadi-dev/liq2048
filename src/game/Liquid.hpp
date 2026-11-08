#pragma once
// Liquid types (v1.8.1): the material of the pot. A type changes how the liquid behaves, not the game rules.
// Water is the reference physics every ranked score, Daily and Puzzle use.
#include <algorithm>

#include "game/World.hpp"

namespace liq {

enum Liquid { kLiqWater, kLiqDilute, kLiqOil, kLiqHoney, kLiqSlime, kLiqMercury, kLiqJelly, kLiqBubble, kLiquidCount };

struct LiquidProps {
    const char* key;
    float thickness;  // viscosity multiplier: how slowly it flows
    float shear;      // neighbours drag each other to a common velocity: it moves as one gooey mass
    float cohesion;   // surface tension of a drop in motion: how well a falling drop holds together
    float adhesion;   // surface tension that stays on at rest (sticky liquids keep their lumps)
    float wallDrag;   // horizontal velocity kept along floor and walls (water 0.97; low = it clings, 1 = frictionless)
    float wallVy;     // vertical velocity kept on landing (low = a plop instead of a splash)
    float jelly;      // shape memory: spring (1/s^2) that pulls a drop back to its round body, so it wobbles
    float jellyDamp;  // damping of the motion inside a drop (1/s)
};

inline const LiquidProps& liquidProps(int t) {
    static const LiquidProps k[kLiquidCount] = {
        {"liquid.water", 1.0f, 0.00f, 1.0f, 0.00f, 0.97f, 0.90f, 0.f, 0.f},
        {"liquid.dilute", 0.25f, 0.00f, 0.6f, 0.00f, 0.97f, 0.90f, 0.f, 0.f},
        {"liquid.oil", 2.2f, 0.03f, 1.3f, 0.10f, 0.90f, 0.75f, 0.f, 0.f},
        {"liquid.honey", 3.0f, 0.05f, 1.5f, 0.00f, 0.50f, 0.30f, 0.f, 0.f},
        {"liquid.slime", 4.0f, 0.10f, 3.0f, 0.30f, 0.20f, 0.10f, 0.f, 0.f},
        {"liquid.mercury", 0.6f, 0.00f, 2.4f, 0.00f, 1.00f, 0.90f, 0.f, 0.f},
        {"liquid.jelly", 1.5f, 0.10f, 2.0f, 0.00f, 0.90f, 0.80f, 900.f, 10.f},
        {"liquid.bubble", 1.0f, 0.20f, 1.0f, 0.00f, 0.96f, 0.50f, 1500.f, 14.f},
    };
    return k[t < 0 || t >= kLiquidCount ? 0 : t];
}

// Sets the world's material. `thickMod` is the Thick liquid modifier (triples the viscosity on top of the type).
inline void applyLiquid(World& w, int type, bool thickMod = false) {
    const LiquidProps& p = liquidProps(type);
    w.viscScale = p.thickness * (thickMod ? 3.f : 1.f);
    w.shear = p.shear;
    w.cohesion = p.cohesion;
    w.restCohesion = p.adhesion;
    w.wallDrag = p.wallDrag;
    w.wallVy = p.wallVy;
    w.jelly = p.jelly;
    w.jellyDamp = p.jellyDamp;
    w.damp = cfg::kDamp;
}

}  // namespace liq
