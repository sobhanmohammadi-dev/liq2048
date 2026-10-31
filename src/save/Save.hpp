#pragma once
// Persistent progress + settings, stored as simple key=value text in the pref dir.
#include <cstdint>
#include <string>

#include "core/Types.hpp"
#include "save/Achievements.hpp"

namespace liq {

class Save {
public:
    int best[kModeCount] = {0, 0, 0, 0, 0};
    float volume = 0.8f;
    bool fullscreen = false;
    std::string lang;           // "", "en", "fa"
    int theme = 0;
    uint32_t ach = 0;           // bitmask of Ach
    int streak = 0;             // consecutive days played (non-punitive: resets quietly)
    int lastDay = 0;
    bool tutorialDone = false;
    int targetStage = 0;        // Target mode escalates 256 -> 512 -> 1024
    int dailyDay = 0;
    int dailyToday = 0;         // best score on dailyDay

    void load();
    void store() const;
    bool has(int id) const { return (ach >> id) & 1u; }
    bool unlock(int id);        // true if newly unlocked
    void touchDay(int today);

private:
    std::string path() const;
};

}  // namespace liq
