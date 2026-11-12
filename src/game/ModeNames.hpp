#pragma once
// Localisation keys of the game modes, indexed by ModeId. One table for the whole code base.
#include "core/Types.hpp"

namespace liq {

inline const char* modeNameKey(ModeId m) {
    static const char* k[kModeCount] = {"mode.classic", "mode.timed",   "mode.target",  "mode.daily",    "mode.zen",
                                        "mode.rain",    "mode.tide",    "mode.weekly",  "mode.puzzle",   "mode.battle",
                                        "mode.classic", "mode.bigbox",  "mode.madhouse", "mode.blind"};
    return k[int(m)];
}
inline const char* modeDescKey(ModeId m) {
    static const char* k[kModeCount] = {"mode.classic.desc", "mode.timed.desc",  "mode.target.desc",  "mode.daily.desc",    "mode.zen.desc",
                                        "mode.rain.desc",    "mode.tide.desc",   "mode.weekly.desc",  "mode.puzzle.desc",   "mode.battle.desc",
                                        "mode.classic.desc", "mode.bigbox.desc", "mode.madhouse.desc", "mode.blind.desc"};
    return k[int(m)];
}
// Slots that exist only so old saves and run codes keep their numbering: Mirror (removed in 1.8.3).
inline bool modeRetired(ModeId m) { return m == ModeId::Mirror; }

}  // namespace liq
