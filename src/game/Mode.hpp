#pragma once
// Game modes are strategies: the scene runs the shared loop and asks the mode
// for rules (seed, drop limit, gravity, win/lose evaluation, HUD status).
#include <cstdint>
#include <memory>
#include <string>

#include "core/Types.hpp"
#include "game/Puzzle.hpp"
#include "i18n/Loc.hpp"
#include "save/Save.hpp"

namespace liq {

enum class Outcome { Running, Won, Lost, Finished };

struct Stats {
    int score = 0;
    int highest = 0;      // highest level reached this run
    int drops = 0;
    int dropsLeft = -1;   // -1 = unlimited
    float elapsed = 0;
    float quiet = 0;      // seconds since the last drop/merge
    bool overflow = false;
};

class Mode {
public:
    virtual ~Mode() = default;
    virtual ModeId id() const = 0;
    virtual const char* nameKey() const = 0;
    virtual uint32_t seed() const { return 0; }      // 0 = random
    virtual int dropLimit() const { return -1; }
    virtual float gravityScale() const { return 1.f; }
    virtual bool failable() const { return true; }
    virtual void update(float) {}
    // Endless Rain: drops fall by themselves; > 0 = seconds between drops, manualDrop() is false.
    virtual float autoDropInterval(float) const { return 0.f; }
    virtual bool manualDrop() const { return true; }
    // Rising Tide: how far (px) the overflow line has sunk after `elapsed` seconds.
    virtual float deadLineShift(float) const { return 0.f; }
    // Puzzle: fixed drop queue (>= 0 = forced level), pre-filled pot, goal. -1 = not a puzzle.
    virtual int queueLevel(int) const { return -1; }
    virtual const PuzzleDef* puzzle() const { return nullptr; }
    // Weekly: the week's special rule (modifier index), -1 = use the player's own modifier.
    virtual int fixedModifier() const { return -1; }
    virtual Outcome evaluate(const Stats& s) const {
        return s.overflow && failable() ? Outcome::Lost : Outcome::Running;
    }
    virtual std::string status(const Loc&, const Stats&) const { return ""; }
    virtual float progress(const Stats&) const { return -1.f; }  // 0..1, or -1 for none
};

std::unique_ptr<Mode> makeMode(ModeId id, const Save& save, int today, int puzzleLevel = 0);
// Battle: a timed match with a shared seed and the chosen modifier (see battle/Battle.hpp).
std::unique_ptr<Mode> makeBattleMode(uint32_t seed, int durationSeconds, int modifier);
// Week number (Monday-based) for a day number, and the rule that week uses (modifier 1..7).
inline int weekOf(int dayNumber) { return (dayNumber + 3) / 7; }
inline int weeklyRuleOf(int week) {  // the week's rule: the physics and rule modifiers 1..7
    return 1 + week % 7;
}

}  // namespace liq
