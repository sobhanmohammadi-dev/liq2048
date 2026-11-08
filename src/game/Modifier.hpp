#pragma once
// Rule modifiers: they change the physics or the rules of any mode. Index 0 is "none".
//   1 low gravity, 2 heavy gravity, 3 thick liquid, 4 bonus drops           (since v1.3)
//   5 wind, 6 earthquake                                                    (v1.8.0, physics)
//   7 fog (the next drop and the aim guide are hidden)                      (v1.8.0)
//   8 downpour (drops fall by themselves; was the mode "Endless Rain"),
//   9 rising tide (the overflow line sinks; was the mode "Rising Tide")     (v1.8.0, moved from modes)
// Everything here is a pure function of the physics step count, so runs and replays stay deterministic.
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "core/Config.hpp"

namespace liq {

enum Modifier { kModNone, kModLowG, kModHeavy, kModThick, kModBonus, kModWind, kModQuake, kModFog, kModRain, kModTide, kModifierCount };
constexpr int kBattleModifiers = 7;  // Battle offers the ones that only change the physics (none .. earthquake)

inline const char* modifierKey(int m) {
    static const char* k[kModifierCount] = {"mod.none", "mod.lowg", "mod.heavy", "mod.thick", "mod.bonus",
                                            "mod.wind", "mod.quake", "mod.fog", "mod.rain", "mod.tide"};
    return k[m < 0 || m >= kModifierCount ? 0 : m];
}

inline float modifierGravity(int m) { return m == kModLowG ? 0.55f : m == kModHeavy ? 1.4f : 1.f; }

// Sideways acceleration (px/s^2) of the wind / earthquake at physics step `step`.
inline float modifierSideways(int m, uint32_t step) {
    const float t = float(step) * cfg::kStep;
    if (m == kModWind) return 110.f * std::sin(t * 6.2831853f / 9.f) + 55.f * std::sin(t * 6.2831853f / 3.7f + 1.f);
    if (m == kModQuake) {  // a 0.9 s tremor every 8 s, building up and fading out
        const float c = std::fmod(t, 8.f);
        if (c >= 0.9f) return 0.f;
        return 1500.f * std::sin(c * 3.1415927f / 0.9f) * std::sin(t * 6.2831853f * 13.f);
    }
    return 0.f;
}
// 0..1 how strong the screen shake of the earthquake is at `step`.
inline float quakeAmount(int m, uint32_t step) {
    if (m != kModQuake) return 0.f;
    const float c = std::fmod(float(step) * cfg::kStep, 8.f);
    return c >= 0.9f ? 0.f : std::sin(c * 3.1415927f / 0.9f);
}

// Downpour: seconds between automatic drops after `elapsed` seconds.
inline float rainInterval(float elapsed) { return std::max(1.1f, 3.2f - elapsed * 0.02f); }
// Rising tide: how far (px) the overflow line has sunk after `elapsed` seconds. It sinks at a steady rate until it is
// kTideMaxShift lower, then keeps moving like a swell so it never looks stuck.
inline float tideShift(float elapsed) {
    const float raw = elapsed * cfg::kTideRate;
    if (raw < cfg::kTideMaxShift) return raw;
    const float t = (raw - cfg::kTideMaxShift) / cfg::kTideRate;
    return cfg::kTideMaxShift + cfg::kTideSwell * std::min(1.f, t / 6.f) * std::sin(t * 6.2831853f / cfg::kTideSwellPeriod);
}

}  // namespace liq
