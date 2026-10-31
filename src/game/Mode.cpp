#include "game/Mode.hpp"

#include <algorithm>
#include <cstdio>

#include "core/Config.hpp"

namespace liq {

namespace {

std::string mmss(float s) {
    int t = int(s + 0.999f);
    if (t < 0) t = 0;
    char b[16];
    std::snprintf(b, sizeof b, "%d:%02d", t / 60, t % 60);
    return b;
}

class Classic final : public Mode {
public:
    ModeId id() const override { return ModeId::Classic; }
    const char* nameKey() const override { return "mode.classic"; }
};

class Timed final : public Mode {
public:
    ModeId id() const override { return ModeId::Timed; }
    const char* nameKey() const override { return "mode.timed"; }
    void update(float dt) override { left_ -= dt; }
    Outcome evaluate(const Stats& s) const override {
        if (s.overflow) return Outcome::Lost;
        return left_ <= 0 ? Outcome::Finished : Outcome::Running;
    }
    std::string status(const Loc& l, const Stats&) const override { return l.tr("hud.time") + " " + mmss(left_); }
    float progress(const Stats&) const override { return std::max(0.f, left_) / cfg::kTimeAttackSeconds; }

private:
    float left_ = cfg::kTimeAttackSeconds;
};

// Reach a tile with limited drops. Escalates 256 -> 512 -> 1024 as you win.
class Target final : public Mode {
public:
    explicit Target(int stage) {
        stage = std::min(std::max(stage, 0), 2);
        level_ = 8 + stage;                       // 256 / 512 / 1024
        limit_ = stage == 0 ? 140 : stage == 1 ? 260 : 520;
    }
    ModeId id() const override { return ModeId::Target; }
    const char* nameKey() const override { return "mode.target"; }
    int dropLimit() const override { return limit_; }
    Outcome evaluate(const Stats& s) const override {
        if (s.highest >= level_) return Outcome::Won;
        if (s.overflow) return Outcome::Lost;
        if (s.dropsLeft == 0 && s.quiet > 1.5f) return Outcome::Lost;
        return Outcome::Running;
    }
    std::string status(const Loc& l, const Stats& s) const override {
        return l.tr("hud.target") + " " + std::to_string(1 << level_) + "  " + l.tr("hud.drops") + " " +
               std::to_string(std::max(0, s.dropsLeft));
    }
    float progress(const Stats& s) const override { return std::min(1.f, float(s.highest + 1) / float(level_ + 1)); }

private:
    int level_, limit_;
};

// Same seed for everyone on a given UTC day, fixed number of drops.
class Daily final : public Mode {
public:
    explicit Daily(int today) : seed_(uint32_t(today) * 2654435761u + 12345u) {}
    ModeId id() const override { return ModeId::Daily; }
    const char* nameKey() const override { return "mode.daily"; }
    uint32_t seed() const override { return seed_ ? seed_ : 1; }
    int dropLimit() const override { return cfg::kDailyDrops; }
    Outcome evaluate(const Stats& s) const override {
        if (s.overflow) return Outcome::Lost;
        if (s.dropsLeft == 0 && s.quiet > 1.5f) return Outcome::Finished;
        return Outcome::Running;
    }
    std::string status(const Loc& l, const Stats& s) const override {
        return l.tr("hud.drops") + " " + std::to_string(std::max(0, s.dropsLeft));
    }
    float progress(const Stats& s) const override { return float(s.dropsLeft < 0 ? 0 : s.dropsLeft) / cfg::kDailyDrops; }

private:
    uint32_t seed_;
};

class Zen final : public Mode {
public:
    ModeId id() const override { return ModeId::Zen; }
    const char* nameKey() const override { return "mode.zen"; }
    float gravityScale() const override { return cfg::kZenGravity; }
    bool failable() const override { return false; }
};

}  // namespace

std::unique_ptr<Mode> makeMode(ModeId id, const Save& save, int today) {
    switch (id) {
        case ModeId::Timed: return std::make_unique<Timed>();
        case ModeId::Target: return std::make_unique<Target>(save.targetStage);
        case ModeId::Daily: return std::make_unique<Daily>(today);
        case ModeId::Zen: return std::make_unique<Zen>();
        default: return std::make_unique<Classic>();
    }
}

}  // namespace liq
