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

// Drops fall on their own, faster and faster; you only steer. Survive as long as you can.
class Rain final : public Mode {
public:
    ModeId id() const override { return ModeId::Rain; }
    const char* nameKey() const override { return "mode.rain"; }
    float autoDropInterval(float elapsed) const override { return std::max(1.1f, 3.2f - elapsed * 0.02f); }
    bool manualDrop() const override { return false; }
    std::string status(const Loc& l, const Stats& s) const override { return l.tr("hud.time") + " " + mmss(s.elapsed - 0.999f); }
};

// The overflow line slowly sinks; merging pushes it back up.
class Tide final : public Mode {
public:
    ModeId id() const override { return ModeId::Tide; }
    const char* nameKey() const override { return "mode.tide"; }
    float deadLineShift(float elapsed) const override { return std::min(cfg::kTideMaxShift, elapsed * cfg::kTideRate); }
    std::string status(const Loc& l, const Stats& s) const override { return l.tr("hud.time") + " " + mmss(s.elapsed - 0.999f); }
};

// Same seed for everyone for a whole week, with that week's special rule.
class Weekly final : public Mode {
public:
    explicit Weekly(int week) : week_(week), seed_(uint32_t(week) * 2246822519u + 777u) {}
    ModeId id() const override { return ModeId::Weekly; }
    const char* nameKey() const override { return "mode.weekly"; }
    uint32_t seed() const override { return seed_ ? seed_ : 1; }
    int dropLimit() const override { return cfg::kWeeklyDrops; }
    int fixedModifier() const override { return weeklyRuleOf(week_); }
    Outcome evaluate(const Stats& s) const override {
        if (s.overflow) return Outcome::Lost;
        if (s.dropsLeft == 0 && s.quiet > 1.5f) return Outcome::Finished;
        return Outcome::Running;
    }
    std::string status(const Loc& l, const Stats& s) const override {
        return l.tr("hud.drops") + " " + std::to_string(std::max(0, s.dropsLeft));
    }
    float progress(const Stats& s) const override { return float(s.dropsLeft < 0 ? 0 : s.dropsLeft) / cfg::kWeeklyDrops; }

private:
    int week_;
    uint32_t seed_;
};

// A pre-filled pot and a fixed queue of drops: reach the goal tile before the queue runs out.
class Puzzle final : public Mode {
public:
    explicit Puzzle(int level) {
        const auto& pack = puzzlePack();
        if (!pack.empty()) def_ = pack[size_t(std::min(std::max(level, 0), int(pack.size()) - 1))];
        else def_.queue = {0, 0, 1};
        number_ = level + 1;
    }
    ModeId id() const override { return ModeId::Puzzle; }
    const char* nameKey() const override { return "mode.puzzle"; }
    uint32_t seed() const override { return 1; }
    int dropLimit() const override { return int(def_.queue.size()); }
    int queueLevel(int i) const override { return i >= 0 && i < int(def_.queue.size()) ? def_.queue[size_t(i)] : -1; }
    const PuzzleDef* puzzle() const override { return &def_; }
    Outcome evaluate(const Stats& s) const override {
        if (s.highest >= def_.goal) return Outcome::Won;
        if (s.overflow) return Outcome::Lost;
        if (s.dropsLeft == 0 && s.quiet > 1.5f) return Outcome::Lost;
        return Outcome::Running;
    }
    std::string status(const Loc& l, const Stats& s) const override {
        return l.tr("hud.target") + " " + std::to_string(1 << def_.goal) + "  " + l.tr("hud.drops") + " " +
               std::to_string(std::max(0, s.dropsLeft));
    }
    float progress(const Stats& s) const override { return std::min(1.f, float(s.highest + 1) / float(def_.goal + 1)); }

private:
    PuzzleDef def_;
    int number_ = 1;
};

}  // namespace

std::unique_ptr<Mode> makeMode(ModeId id, const Save& save, int today, int puzzleLevel) {
    switch (id) {
        case ModeId::Timed: return std::make_unique<Timed>();
        case ModeId::Target: return std::make_unique<Target>(save.targetStage);
        case ModeId::Daily: return std::make_unique<Daily>(today);
        case ModeId::Zen: return std::make_unique<Zen>();
        case ModeId::Rain: return std::make_unique<Rain>();
        case ModeId::Tide: return std::make_unique<Tide>();
        case ModeId::Weekly: return std::make_unique<Weekly>(weekOf(today));
        case ModeId::Puzzle: return std::make_unique<Puzzle>(puzzleLevel);
        default: return std::make_unique<Classic>();
    }
}

}  // namespace liq
