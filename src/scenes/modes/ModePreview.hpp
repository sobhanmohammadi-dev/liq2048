#pragma once
// ModePreview: the looping, non-interactive "video clip" shown on the mode information page.
//
// Every mode has its own scripted clip that runs on the real liquid physics (a small World per pot), so the clip
// looks exactly like the game: drops are aimed and dropped on a schedule, merge, and get mode specific decoration
// (clock, calendar, fog, camera frame, rival pot ...). The clip is a pure function of its own clock: it never
// reads input and it loops forever (a short fade hides the cut).
#include <string>
#include <vector>

#include "game/World.hpp"
#include "scenes/Ctx.hpp"

namespace liq {

class ModePreview {
public:
    static constexpr float kW = 222.f;  // size of the clip frame in logical pixels
    static constexpr float kH = 178.f;

    // Advances the clip by dt seconds and draws it with its top-left corner at (x, y).
    void draw(Ctx& c, ModeId mode, float x, float y, float dt);

private:
    struct Drp {  // one scheduled drop: when, where (0..1 across the pot), level and pot
        float t, u;
        int lv, pot;
    };
    struct Spark {
        float x, y, vx, vy, life, max;
        Color c;
    };
    struct Ring {
        float x, y, age;
        Color c;
    };
    struct Floater {
        float x, y, life;
        std::string s;
    };

    void reset(Ctx& c, ModeId mode);
    void tick(Ctx& c);
    void spawnPlanned(Ctx& c);
    void onMerge(const Theme& t, int pot, const MergeEvent& e);
    void drawScene(Ctx& c, float x, float y);
    void drawPot(Ctx& c, int pot, float warn) const;
    void drawDeco(Ctx& c, float x, float y);
    void drawFx(Ctx& c) const;

    float potX(int pot) const { return fx_ + (pots_ == 2 ? 8.f + float(pot) * 106.f : (kW - potW_) / 2.f); }
    float potY() const { return fy_ + kH - potH_ - 14.f; }

    ModeId mode_ = ModeId::Classic;
    bool ready_ = false;
    float fx_ = 0.f, fy_ = 0.f;  // frame origin of the current draw
    float acc_ = 0.f, simT_ = 0.f, loop_ = 10.f, gravity_ = 1.f;
    int pots_ = 1, planIdx_ = 0, score_ = 0, shuffles_ = 0, merges_ = 0;
    float potW_ = 150.f, potH_ = 118.f, shake_ = 0.f, flash_ = 0.f;
    bool wind_ = false;
    World w_[2];
    std::vector<Drp> plan_;
    std::vector<Spark> sparks_;
    std::vector<Ring> rings_;
    std::vector<Floater> floaters_;
    uint32_t step_ = 0;
};

}  // namespace liq
