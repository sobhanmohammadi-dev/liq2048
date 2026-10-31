#pragma once
// Game modes are strategies: the scene runs the shared loop and asks the mode
// for rules (seed, drop limit, gravity, win/lose evaluation, HUD status).
#include <cstdint>
#include <memory>
#include <string>

#include "core/Types.hpp"
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
    virtual void update(float dt) {}
    virtual Outcome evaluate(const Stats& s) const {
        return s.overflow && failable() ? Outcome::Lost : Outcome::Running;
    }
    virtual std::string status(const Loc&, const Stats&) const { return ""; }
    virtual float progress(const Stats&) const { return -1.f; }  // 0..1, or -1 for none
};

std::unique_ptr<Mode> makeMode(ModeId id, const Save& save, int today);

}  // namespace liq
