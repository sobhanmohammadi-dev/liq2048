#pragma once
// Persistent progress + settings, stored as simple key=value text in the pref dir.
#include <cstdint>
#include <string>

#include "core/Types.hpp"
#include "save/Achievements.hpp"

namespace liq {

class Save {
public:
    // Frame-rate choices: 0 = VSync (match the display), -1 = unlimited, otherwise a cap in FPS.
    static constexpr int kFpsChoiceCount = 7;
    static int fpsChoice(int idx) {
        constexpr int v[kFpsChoiceCount] = {0, 30, 60, 90, 120, 144, -1};
        return v[idx < 0 ? 0 : (idx >= kFpsChoiceCount ? kFpsChoiceCount - 1 : idx)];
    }

    int best[kModeCount] = {};
    float musicVol = 0.6f;      // 0..1
    float sfxVol = 0.9f;        // 0..1
    bool fullscreen = false;
    std::string lang;           // "" (system) or a code from kLangs
    int theme = 0;
    uint32_t ach = 0;           // bitmask of Ach
    int streak = 0;             // consecutive days played (non-punitive: resets quietly)
    int lastDay = 0;
    bool tutorialDone = false;
    int targetStage = 0;        // Target mode escalates 256 -> 512 -> 1024
    int dailyDay = 0;
    int dailyToday = 0;         // best score on dailyDay
    // Modes (v1.3.0)
    static constexpr int kPuzzleMax = 60;
    int modifier = 0;           // 0 none, 1 low gravity, 2 heavy gravity, 3 thick liquid, 4 bonus drops
    int weeklyWeek = 0, weeklyBest = 0;
    unsigned char puzzleStars[kPuzzleMax] = {};  // 0 = unsolved, 1..3 stars
    uint32_t seenModes = 0;     // bit per ModeId: intro hint already shown
    // Lifetime statistics
    int gamesPlayed = 0, totalDrops = 0, totalMerges = 0, longestChain = 0, highestLevel = 0;
    int playSeconds = 0;
    int puzzlesSolved() const {
        int n = 0;
        for (unsigned char s : puzzleStars) n += s > 0;
        return n;
    }
    int fpsIdx = 0;             // index into fpsChoice(); default VSync
    bool showFps = false;
    // Game feel (v1.1.0)
    bool haptics = true;        // vibration on phones
    bool guide = true;          // dotted aim line + landing ring
    bool shake = true;          // screen shake
    bool slowmo = true;         // brief slow motion on big merges
    bool calm = false;          // calm mode: no shake, slow motion, blinking or flashes
    // Accessibility (v1.2.0)
    int quality = 1;            // liquid quality: 0 low (1 substep), 1 medium (2), 2 high (3)
    int colorVision = 0;        // 0 standard, 1 red/green safe, 2 blue/yellow safe
    bool highContrast = false;
    bool reduceMotion = false;  // no transitions, particles, shake, slow motion or drifting backdrop
    bool leftHand = false;      // mirrors the in-game HUD
    bool bigTouch = false;      // larger invisible touch margins around buttons
    std::string altLang;        // language the quick toggle switches to (besides English)
    bool effShake() const { return shake && !calm && !reduceMotion; }
    bool effSlowmo() const { return slowmo && !calm && !reduceMotion; }

    static std::string& pathOverride() {  // tests point the save at a scratch file
        static std::string p;
        return p;
    }
    int fpsLimit() const { return fpsChoice(fpsIdx); }
    void load();
    void store() const;
    bool has(int id) const { return (ach >> id) & 1u; }
    bool unlock(int id);        // true if newly unlocked
    void touchDay(int today);

private:
    std::string path() const;
};

}  // namespace liq
