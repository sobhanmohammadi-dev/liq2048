#include "scenes/modes/ModePreview.hpp"

#include <algorithm>
#include <cmath>

#include "core/Config.hpp"
#include "core/Util.hpp"
#include "game/Liquid.hpp"
#include "game/Modifier.hpp"
#include "scenes/Scenes.hpp"

namespace liq {

namespace {

constexpr float kTau = 6.2831853f;
constexpr float kFade = 0.35f;  // seconds of fade-in / fade-out around the loop point

float sstep(float a, float b, float x) {
    const float u = clampv((x - a) / (b - a), 0.f, 1.f);
    return u * u * (3.f - 2.f * u);
}
float pulse(float p) { return 0.5f - 0.5f * std::cos(p * kTau); }

std::string clock(float seconds) {  // "1:30"
    const int s = std::max(0, int(std::ceil(seconds)));
    return std::to_string(s / 60) + ":" + (s % 60 < 10 ? "0" : "") + std::to_string(s % 60);
}

void dotted(Gfx& g, float x, float y0, float y1, Color c) {
    for (float y = y0; y < y1; y += 6.f) g.rect(std::floor(x), std::floor(y), 1, 3, c);
}

}  // namespace

// ---- scripts ------------------------------------------------------------------------------------------------------------
void ModePreview::reset(Ctx& c, ModeId mode) {
    mode_ = mode;
    ready_ = true;
    simT_ = acc_ = 0.f;
    planIdx_ = score_ = shuffles_ = merges_ = 0;
    shake_ = flash_ = 0.f;
    step_ = 0;
    sparks_.clear();
    rings_.clear();
    floaters_.clear();
    plan_.clear();
    loop_ = 10.f;
    gravity_ = 1.f;
    pots_ = 1;
    potW_ = 150.f;
    potH_ = 118.f;
    wind_ = false;

    // Drop schedules: {time, position across the pot, level, pot}
    switch (mode) {
        case ModeId::Classic:
            plan_ = {{0.6f, .30f, 0, 0}, {1.9f, .36f, 0, 0}, {3.4f, .75f, 0, 0}, {4.5f, .70f, 0, 0}, {6.0f, .50f, 1, 0}, {7.4f, .25f, 0, 0}, {8.4f, .30f, 0, 0}};
            break;
        case ModeId::Zen:
            gravity_ = 0.45f;
            loop_ = 12.f;
            plan_ = {{0.8f, .4f, 0, 0}, {3.0f, .55f, 0, 0}, {5.2f, .45f, 1, 0}, {7.6f, .65f, 0, 0}, {9.6f, .5f, 0, 0}};
            break;
        case ModeId::Timed:
            plan_ = {{0.4f, .3f, 0, 0}, {1.2f, .35f, 0, 0}, {2.0f, .7f, 0, 0}, {2.8f, .75f, 0, 0}, {3.6f, .5f, 1, 0}, {4.5f, .2f, 0, 0},
                     {5.3f, .25f, 0, 0}, {6.1f, .8f, 0, 0}, {6.9f, .78f, 0, 0}, {7.7f, .45f, 1, 0}, {8.5f, .6f, 0, 0}};
            break;
        case ModeId::Target:
            plan_ = {{0.5f, .3f, 1, 0}, {1.6f, .35f, 1, 0}, {3.0f, .7f, 1, 0}, {4.0f, .66f, 1, 0}, {5.4f, .5f, 2, 0}, {6.4f, .45f, 2, 0}};
            break;
        case ModeId::Daily:
            plan_ = {{0.9f, .3f, 0, 0}, {2.0f, .34f, 0, 0}, {3.4f, .7f, 1, 0}, {4.8f, .68f, 1, 0}, {6.2f, .4f, 0, 0}, {7.4f, .5f, 0, 0}};
            break;
        case ModeId::Weekly:
            wind_ = true;
            plan_ = {{0.6f, .3f, 0, 0}, {1.8f, .4f, 0, 0}, {3.2f, .65f, 1, 0}, {4.6f, .6f, 1, 0}, {6.0f, .35f, 0, 0}, {7.4f, .5f, 0, 0}};
            break;
        case ModeId::Puzzle:
            loop_ = 11.f;
            plan_ = {{0.0f, .16f, 1, 0}, {0.1f, .50f, 1, 0}, {0.2f, .84f, 1, 0}, {0.9f, .30f, 0, 0}, {1.0f, .70f, 0, 0}, {3.2f, .33f, 2, 0}, {5.0f, .67f, 2, 0}};
            break;
        case ModeId::Battle:
            pots_ = 2;
            potW_ = 100.f;
            potH_ = 108.f;
            plan_ = {{0.5f, .3f, 0, 0}, {1.4f, .7f, 0, 1}, {1.8f, .35f, 0, 0}, {2.7f, .6f, 0, 1}, {3.3f, .7f, 0, 0}, {4.1f, .35f, 0, 1},
                     {4.8f, .5f, 1, 0}, {5.9f, .5f, 1, 1}, {6.6f, .3f, 0, 0}, {7.4f, .75f, 0, 1}};
            break;
        case ModeId::BigBox:
            loop_ = 11.f;
            potW_ = 200.f;
            potH_ = 104.f;
            plan_ = {{0.5f, .12f, 0, 0}, {1.4f, .38f, 0, 0}, {2.3f, .62f, 0, 0}, {3.2f, .88f, 0, 0}, {4.4f, .15f, 1, 0}, {5.3f, .40f, 0, 0},
                     {6.2f, .65f, 1, 0}, {7.1f, .85f, 0, 0}, {8.0f, .5f, 0, 0}};
            break;
        case ModeId::Madhouse:
            loop_ = 10.f;
            plan_ = {{0.4f, .3f, 0, 0}, {1.2f, .7f, 1, 0}, {2.0f, .5f, 0, 0}, {3.4f, .25f, 1, 0}, {4.2f, .75f, 0, 0}, {5.6f, .5f, 1, 0},
                     {6.4f, .3f, 0, 0}, {7.8f, .7f, 0, 0}};
            break;
        case ModeId::Blind:
            plan_ = {{0.5f, .3f, 0, 0}, {1.6f, .35f, 0, 0}, {2.8f, .7f, 1, 0}, {4.0f, .65f, 1, 0}, {5.4f, .5f, 0, 0}, {6.6f, .3f, 0, 0}, {7.8f, .6f, 1, 0}};
            break;
        default: plan_ = {{0.6f, .4f, 0, 0}, {2.0f, .5f, 0, 0}}; break;
    }
    for (int i = 0; i < pots_; ++i) {
        World& w = w_[i];
        w.clear();
        w.fieldL = 0.f;
        w.fieldR = potW_;
        w.floorY = potH_;
        w.worldW = potW_ + 30.f;
        w.worldH = potH_ + 60.f;
        applyLiquid(w, c.save.liquid);
        w.substeps = 2;
    }
}

void ModePreview::spawnPlanned(Ctx& c) {
    while (planIdx_ < int(plan_.size()) && plan_[size_t(planIdx_)].t <= simT_) {
        const Drp& d = plan_[size_t(planIdx_++)];
        const float r = World::radiusFor(d.lv), x = r + 2.f + (potW_ - 2.f * r - 4.f) * d.u;
        w_[d.pot].spawn(x, mode_ == ModeId::Puzzle && d.t < 0.5f ? potH_ - 26.f : 8.f, d.lv);
    }
    (void)c;
}

void ModePreview::onMerge(const Theme& t, int pot, const MergeEvent& e) {
    const int v = (1 << e.level) * e.chain;
    if (pot == 0) score_ += v;
    ++merges_;
    const float ox = potX(pot) - fx_, oy = potY() - fy_;
    rings_.push_back({ox + e.x, oy + e.y, 0.f, t.blob[std::min(e.level, 11)]});
    for (int i = 0; i < 6; ++i) {
        const float a = float(i) * 1.047f + float(merges_) * 0.7f;
        sparks_.push_back({ox + e.x, oy + e.y, std::cos(a) * 40.f, std::sin(a) * 40.f - 20.f, 0.5f, 0.5f, t.blob[std::min(e.level + 1, 11)]});
    }
    floaters_.push_back({ox + e.x, oy + e.y - World::radiusFor(e.level) - 4.f, 0.9f, "+" + std::to_string(v)});
}

void ModePreview::tick(Ctx& c) {
    const Theme& t = *c.theme;
    spawnPlanned(c);
    for (int i = 0; i < pots_; ++i) {
        World& w = w_[i];
        const float gx = wind_ ? modifierSideways(kModWind, step_) * 0.7f : 0.f;
        w.step(cfg::kStep, cfg::kGravity * gravity_, gx);
        for (const MergeEvent& e : w.events) onMerge(t, i, e);
        for (const Impact& im : w.impacts)
            if (mode_ == ModeId::Zen && im.strength > 0.1f) rings_.push_back({potX(i) - fx_ + im.x, potY() - fy_ + im.y, 0.f, t.blob[0]});
        w.events.clear();
        w.impacts.clear();
    }
    // Madhouse: every 2.5 s every drop changes level and the screen flashes
    if (mode_ == ModeId::Madhouse && simT_ > 0.1f && int(simT_ / 2.5f) != int((simT_ - cfg::kStep) / 2.5f)) {
        ++shuffles_;
        flash_ = 1.f;
        shake_ = 1.f;
        World& w = w_[0];
        for (size_t i = 0; i < w.drops.size(); ++i) {  // like the real Madhouse: a step up or down
            const int lv = clampv(w.drops[i].level + ((int(i) + shuffles_) % 2 ? 1 : -1), 0, 4);
            if (lv != w.drops[i].level) w.relevel(int(i), lv);
        }
    }
    ++step_;
}

// ---- drawing ------------------------------------------------------------------------------------------------------------
void ModePreview::drawPot(Ctx& c, int pot, float warn) const {
    Gfx& g = c.gfx;
    const Theme& t = *c.theme;
    const float L = potX(pot), top = potY(), R = L + potW_, fl = top + potH_;
    g.rect(L, top, potW_, potH_, withAlpha(t.ink, 105));
    for (float x = L; x < R; x += 6.f) g.rect(std::floor(x), top + 6.f, 3, warn > 0.f ? 2 : 1, withAlpha(t.danger, warn > 0.f ? 255 : 150));
    g.rect(L - 4, fl, potW_ + 8, 6, t.frame);
    g.rect(L - 4, fl, potW_ + 8, 1, t.ink);
    g.rect(L - 3, fl + 1, potW_ + 6, 1, t.frameHi);
    g.rect(L - 3, fl + 4, potW_ + 6, 1, t.frameLo);
    g.rect(L - 4, fl + 5, potW_ + 8, 1, t.ink);
    for (float wx : {L - 4.f, R}) {
        g.rect(wx, top, 1, potH_ + 6.f, t.ink);
        g.rect(wx + 1, top, 2, potH_ + 5.f, t.frame);
        g.rect(wx + 3, top, 1, potH_ + 6.f, t.ink);
    }
}

void ModePreview::drawFx(Ctx& c) const {
    Gfx& g = c.gfx;
    const Theme& t = *c.theme;
    for (const Ring& r : rings_) {
        const float u = r.age / 0.5f;
        g.ring(fx_ + r.x, fy_ + r.y, 4 + int(u * 16.f), withAlpha(r.c, int(200.f * (1.f - u))));
    }
    for (const Spark& s : sparks_) g.rect(std::floor(fx_ + s.x), std::floor(fy_ + s.y), 2, 2, withAlpha(s.c, int(255.f * s.life / s.max)));
    for (const Floater& f : floaters_) {
        const float u = 1.f - f.life / 0.9f;
        g.number(f.s, fx_ + f.x, fy_ + f.y - u * 18.f, 1, withAlpha(t.text, int(255.f * (1.f - u * u))), Align::Center, TextFx::Outline, t.ink);
    }
}

void ModePreview::drawDeco(Ctx& c, float x, float y) {
    Gfx& g = c.gfx;
    const Theme& t = *c.theme;
    const float T = simT_, cx = x + kW / 2.f;
    const Color acc = t.accent;
    auto topScore = [&](float sx, Align a) {
        g.text("SCORE", sx, y + 10.f, 1, t.dim, a);
        g.number(std::to_string(score_), sx, y + 24.f, 1, t.text, a, TextFx::Outline, t.ink);
    };
    switch (mode_) {
        case ModeId::Classic: {  // score and the next drop
            topScore(x + 8.f, Align::Left);
            const int nxt = planIdx_ < int(plan_.size()) ? plan_[size_t(planIdx_)].lv : 0;
            g.text("NEXT", x + kW - 22.f, y + 10.f, 1, t.dim, Align::Center);
            g.disc(x + kW - 22.f, y + 28.f, 9, t.ink);
            g.disc(x + kW - 22.f, y + 28.f, 8, t.blob[nxt]);
            break;
        }
        case ModeId::Zen: {  // moon, fireflies, a breathing ring: nothing to lose
            g.disc(x + kW - 30.f, y + 28.f, 14, withAlpha(t.text, 40));
            g.disc(x + kW - 30.f, y + 28.f, 10, withAlpha(t.text, 90));
            for (int i = 0; i < 9; ++i) {
                const float fxp = x + 14.f + std::fmod(float(i) * 61.f + T * (4.f + float(i % 3)), kW - 28.f);
                const float fyp = y + 40.f + std::fmod(float(i) * 37.f, 70.f) + std::sin(T * 0.9f + float(i)) * 5.f;
                g.rect(std::floor(fxp), std::floor(fyp), 2, 2, withAlpha(t.accent, 90 + int(120.f * pulse(std::fmod(T * 0.4f + float(i) * 0.17f, 1.f)))));
            }
            const float br = 7.f + 4.f * std::sin(T * 0.9f);
            g.ring(x + 30.f, y + 26.f, int(br), withAlpha(t.text, 110));
            g.ring(x + 30.f, y + 26.f, int(br * 0.55f), withAlpha(t.text, 70));
            break;
        }
        case ModeId::Timed: {  // draining clock bar
            const float left = 1.f - T / loop_;
            const bool low = left < 0.25f;
            g.rect(x + 28.f, y + 9.f, kW - 56.f, 7, t.ink);
            g.rect(x + 29.f, y + 10.f, (kW - 58.f) * left, 5, low ? mix(t.danger, t.text, 0.5f * pulse(std::fmod(T * 3.f, 1.f))) : acc);
            g.disc(x + 16.f, y + 13.f, 8, t.ink);
            g.disc(x + 16.f, y + 13.f, 7, mix(t.panelHi, t.text, 0.15f));
            const float a = T * 5.f;
            g.rect(std::floor(x + 16.f + std::sin(a) * 5.f), std::floor(y + 13.f - std::cos(a) * 5.f), 2, 2, t.danger);
            g.rect(x + 15.f, y + 9.f, 2, 5, t.ink);
            g.number(clock(90.f * left), x + kW - 24.f, y + 28.f, 1, low ? t.danger : t.text, Align::Center, TextFx::Outline, t.ink);
            g.number(std::to_string(score_), x + 24.f, y + 28.f, 1, t.text, Align::Center, TextFx::Outline, t.ink);
            break;
        }
        case ModeId::Target: {  // the target tile and the drops that are left
            const float cxp = x + 34.f;
            const bool done = T > 7.f;
            const float glow = pulse(std::fmod(T * 0.8f, 1.f));
            g.disc(cxp, y + 28.f, 17 + int(3.f * glow), withAlpha(t.blob[3], 50));
            g.disc(cxp, y + 28.f, 13, t.ink);
            g.disc(cxp, y + 28.f, 12, t.blob[3]);
            g.number("8", cxp, y + 28.f, 1, t.ink, Align::Center);
            g.ring(cxp, y + 28.f, 16, withAlpha(t.text, 160), 2);
            g.text("DROPS", x + kW - 30.f, y + 12.f, 1, t.dim, Align::Center);
            g.number(std::to_string(std::max(0, 12 - planIdx_ * 2)), x + kW - 30.f, y + 28.f, 2, t.text, Align::Center, TextFx::Outline, t.ink);
            if (done) {  // goal reached: confetti, a check mark and a flash
                const float u = sstep(7.f, 7.6f, T);
                g.rect(x, y, kW, kH, withAlpha(t.text, int(40.f * (1.f - u))));
                g.icon(IconId::Check, cx - 4.f, y + 60.f - 8.f * u, acc, 2);
                for (int i = 0; i < 18; ++i) {
                    const float a = float(i) * 0.35f, r = 12.f + (T - 7.f) * 55.f;
                    g.rect(std::floor(cx + std::cos(a) * r * (0.6f + 0.4f * std::sin(float(i)))), std::floor(y + 80.f + std::sin(a) * r * 0.7f + (T - 7.f) * 30.f), 2, 2, t.blob[(i * 5) % 12]);
                }
            }
            break;
        }
        case ModeId::Daily: {  // a calendar page that flips to today, and the streak flame
            const float flip = sstep(0.f, 0.5f, T);
            const float w = 34.f, h = 32.f * (0.15f + 0.85f * flip), bx = x + 8.f, by = y + 8.f;
            g.rect(bx, by, w, h, mix(t.panelHi, t.text, 0.12f));
            g.border(bx, by, w, h, 1, t.ink);
            g.rect(bx + 1, by + 1, w - 2, std::min(8.f, h - 2.f), t.danger);
            if (flip > 0.9f) g.number("9", bx + w / 2.f, by + 21.f, 1, t.ink, Align::Center);
            g.icon(IconId::Crown, x + kW - 28.f, y + 10.f, acc);
            g.number(T > 6.f ? "8" : "7", x + kW - 11.f, y + 18.f, 1, t.text, Align::Right, TextFx::Outline, t.ink);
            break;
        }
        case ModeId::Weekly: {  // the seven days, and wind arrows showing this week's rule
            const int on = int(T / loop_ * 7.f);
            for (int i = 0; i < 7; ++i) {
                const float bx = cx - 7.f * 11.f / 2.f + float(i) * 11.f;
                g.rect(bx, y + 9.f, 9, 9, i == on ? acc : t.panelLo);
                g.border(bx, y + 9.f, 9, 9, 1, t.ink);
            }
            const float dir = modifierSideways(kModWind, step_) >= 0.f ? 1.f : -1.f;  // arrows drift with the wind
            for (int i = 0; i < 3; ++i) {
                const float ax = x + std::fmod(T * 34.f * dir + float(i) * 77.f + 222.f * 4.f, kW), ay = y + 36.f + float(i) * 6.f;
                g.rect(std::floor(ax), ay, 9, 1, withAlpha(t.text, 90));
                g.rect(std::floor(ax) + (dir > 0.f ? 7.f : 0.f), ay - 1.f, 2, 3, withAlpha(t.text, 90));
            }
            break;
        }
        case ModeId::Puzzle: {  // the goal line and three stars that light up
            const float gy = potY() + 34.f;
            dotted(g, potX(0) + 1.f, gy, gy + 1.f, t.accent);
            for (float xx = potX(0); xx < potX(0) + potW_; xx += 6.f) g.rect(std::floor(xx), gy, 3, 1, withAlpha(t.accent, 200));
            g.disc(potX(0) + potW_ + 14.f, gy, 9, t.ink);
            g.disc(potX(0) + potW_ + 14.f, gy, 8, t.blob[3]);
            for (int i = 0; i < 3; ++i) {
                const float tt = 6.2f + float(i) * 1.2f, u = sstep(tt, tt + 0.35f, T);
                const float sx = cx + float(i - 1) * 16.f, sy = y + 18.f;
                const int s = 5 + int(3.f * u * (1.f - sstep(tt + 0.35f, tt + 0.7f, T) * 0.4f));
                g.rect(sx - float(s) / 2.f, sy - float(s) / 2.f, float(s), float(s), u > 0.f ? acc : t.panelLo);
                g.border(sx - float(s) / 2.f, sy - float(s) / 2.f, float(s), float(s), 1, t.ink);
            }
            break;
        }
        case ModeId::Battle: {  // score bars, VS and the attack drop flying across
            const float a = clampv(float(score_) / 40.f, 0.f, 1.f), b = clampv(float(merges_) / 7.f * 0.8f + 0.1f * std::sin(T), 0.f, 1.f);
            for (int s = 0; s < 2; ++s) {
                const float bx = s == 0 ? x + 8.f : x + kW - 8.f - 80.f;
                g.rect(bx, y + 9.f, 80, 6, t.ink);
                g.rect(bx + 1, y + 10.f, 78.f * (s == 0 ? a : b), 4, s == 0 ? t.blob[0] : t.blob[5]);
            }
            g.text("VS", cx, y + 12.f, 1, t.accent, Align::Center, TextFx::Outline, t.ink);
            g.number(clock(60.f - T * 4.f), cx, y + 26.f, 1, t.text, Align::Center, TextFx::Outline, t.ink);
            const float at = sstep(4.2f, 5.1f, T) * (1.f - sstep(5.1f, 5.15f, T));  // projectile 4.2 -> 5.1 s
            if (at > 0.f) {
                const float u = (T - 4.2f) / 0.9f, px = potX(0) + 50.f + (potX(1) - potX(0)) * u, py = y + 42.f - std::sin(u * 3.1416f) * 22.f;
                for (int k = 1; k < 6; ++k) g.disc(px - float(k) * 6.f * (potX(1) - potX(0)) / 106.f, py + float(k) * 1.5f, std::max(1, 5 - k), withAlpha(t.blob[1], 120 - k * 18));
                g.disc(px, py, 7, t.ink);
                g.disc(px, py, 6, t.blob[1]);
            }
            break;
        }
        case ModeId::BigBox: {  // player chips and the camera frame sweeping the wide pot
            const int turn = std::min(3, planIdx_) % 4;
            for (int i = 0; i < 4; ++i) {
                const float bx = x + 18.f + float(i) * 50.f, on = i == (planIdx_ - 1 + 4) % 4 ? 1.f : 0.f;
                g.rect(bx, y + 8.f, 46, 14, on > 0.f ? mix(t.panelHi, t.blob[(i * 3) % 12], 0.5f) : t.panel);
                g.border(bx, y + 8.f, 46, 14, 1, on > 0.f ? acc : t.ink);
                g.disc(bx + 8.f, y + 15.f, 3, t.blob[(i * 3) % 12]);
                g.text("P" + std::to_string(i + 1), bx + 14.f, y + 15.f, 1, on > 0.f ? t.text : t.dim);
            }
            (void)turn;
            const float fw = 112.f, fxp = potX(0) + (potW_ - fw) * (0.5f + 0.5f * std::sin(T * 0.7f));
            g.border(fxp, potY() - 4.f, fw, potH_ + 8.f, 1, acc);
            g.rect(fxp, potY() - 4.f, 6, 1, t.text);
            g.rect(fxp + fw - 6.f, potY() + potH_ + 3.f, 6, 1, t.text);
            break;
        }
        case ModeId::Madhouse: {  // shuffle timer
            const float u = std::fmod(T, 2.5f) / 2.5f;
            g.ring(x + kW - 20.f, y + 22.f, 11, t.ink);
            for (int i = 0; i < int(u * 24.f); ++i) {
                const float a = float(i) / 24.f * kTau - 1.5708f;
                g.rect(std::floor(x + kW - 20.f + std::cos(a) * 10.f), std::floor(y + 22.f + std::sin(a) * 10.f), 2, 2, acc);
            }
            g.number(std::to_string(10 - int(u * 10.f)), x + kW - 20.f, y + 22.f, 1, t.text, Align::Center);
            topScore(x + 8.f, Align::Left);
            if (flash_ > 0.f) g.text("?!", cx, y + 30.f + (1.f - flash_) * 8.f, 2, withAlpha(acc, int(255.f * flash_)), Align::Center, TextFx::Outline, t.ink);
            break;
        }
        case ModeId::Blind: {  // an eye and three peeks
            const float lid = 0.2f + 0.8f * (T > 3.f && T < 4.4f ? 1.f : T > 7.f && T < 8.2f ? 1.f : 0.f);
            const float ex = x + kW - 26.f, ey = y + 22.f;
            for (int dx = -12; dx <= 12; ++dx) {
                const float uu = float(dx) / 12.f, hh = 7.f * lid * std::sqrt(std::max(0.f, 1.f - uu * uu));
                g.rect(ex + float(dx), ey - hh, 1, 2.f * hh + 1.f, mix(t.panelHi, t.text, 0.55f));
                g.rect(ex + float(dx), ey - hh - 1.f, 1, 1, t.ink);
                g.rect(ex + float(dx), ey + hh + 1.f, 1, 1, t.ink);
            }
            g.disc(ex, ey, std::max(1, int(3.f * lid)), t.ink);
            const int left = 3 - (T > 3.f ? 1 : 0) - (T > 7.f ? 1 : 0);
            for (int i = 0; i < 3; ++i) g.rect(x + 10.f + float(i) * 9.f, y + 18.f, 6, 6, i < left ? acc : t.panelLo);
            g.text("PEEK", x + 28.f, y + 10.f, 1, t.dim, Align::Center);
            break;
        }
        default: break;
    }
}

void ModePreview::drawScene(Ctx& c, float x, float y) {
    Gfx& g = c.gfx;
    const Theme& t = *c.theme;
    const int ci = (int(mode_) * 5 + 3) % 12;
    // background: the theme's sky, tinted with the mode's colour (Madhouse shifts its tint at every shuffle)
    const Color tint = mode_ == ModeId::Madhouse ? t.blob[(shuffles_ * 3 + 5) % 12] : t.blob[ci];
    for (float yy = 0.f; yy < kH; yy += 3.f) {
        const float u = yy / kH;
        g.rect(x, y + yy, kW, 3.f, mix(mix(t.sky[0], t.sky[2], u), tint, mode_ == ModeId::Zen ? 0.06f : 0.12f));
    }
    for (int i = 0; i < 16; ++i) {  // twinkling stars deep in the background
        const float sx0 = x + std::fmod(float(i) * 97.f + 11.f, kW - 4.f), sy0 = y + 6.f + std::fmod(float(i) * 53.f, kH * 0.55f);
        g.rect(std::floor(sx0), std::floor(sy0), 1, 1, withAlpha(t.text, 40 + int(70.f * pulse(std::fmod(simT_ * 0.3f + float(i) * 0.37f, 1.f)))));
    }
    const float sx = shake_ > 0.f ? std::floor(std::sin(simT_ * 60.f) * 3.f * shake_) : 0.f;
    fx_ = x + sx;
    fy_ = y;
    for (int i = 0; i < pots_; ++i) drawPot(c, i, w_[i].overflow());

    // liquid
    std::vector<BlobSprite> pile, labels, held;
    for (int i = 0; i < pots_; ++i) {
        const size_t p0 = pile.size(), l0 = labels.size();
        w_[i].particleSprites(1.f, 0.f, pile);
        w_[i].labelSprites(labels);
        for (size_t k = p0; k < pile.size(); ++k) {
            pile[k].x += potX(i);
            pile[k].y += potY();
        }
        for (size_t k = l0; k < labels.size(); ++k) {
            labels[k].x += potX(i);
            labels[k].y += potY();
        }
    }
    // the drop being aimed: it slides to its spot over the 0.7 s before it is released
    for (size_t i = 0; i < plan_.size(); ++i) {
        const Drp& d = plan_[i];
        if (int(i) < planIdx_ || simT_ < d.t - 0.7f || d.t < 0.5f) continue;
        const float r = World::radiusFor(d.lv), tx = r + 2.f + (potW_ - 2.f * r - 4.f) * d.u;
        float from = potW_ / 2.f;
        for (int k = int(i) - 1; k >= 0; --k)
            if (plan_[size_t(k)].pot == d.pot) {
                const float rk = World::radiusFor(plan_[size_t(k)].lv);
                from = rk + 2.f + (potW_ - 2.f * rk - 4.f) * plan_[size_t(k)].u;
                break;
            }
        const float u = sstep(d.t - 0.7f, d.t - 0.1f, simT_), hx = from + (tx - from) * u;
        BlobSprite b = plainSprite(potX(d.pot) + hx, potY() + 8.f, d.lv, 0.05f, simT_);
        const float grow = sstep(d.t - 0.7f, d.t - 0.5f, simT_);
        b.ra *= grow;
        b.rb *= grow;
        if (b.ra > 2.f) held.push_back(b);
        if (c.save.guide && u > 0.6f) dotted(g, std::floor(potX(d.pot) + hx), potY() + 8.f + r + 2.f, potY() + w_[d.pot].surfaceY(hx, r) - 3.f, withAlpha(t.text, 80));
    }
    const bool blind = mode_ == ModeId::Blind;
    const float peek = std::max(sstep(2.8f, 3.2f, simT_) * (1.f - sstep(4.2f, 4.6f, simT_)), sstep(6.8f, 7.2f, simT_) * (1.f - sstep(8.0f, 8.4f, simT_)));
    c.raster.clear();
    c.raster.drawLiquid(pile, t, 9, 6, 1.1f);
    if (!blind) c.raster.drawLabels(labels, t);
    if (!held.empty()) {
        c.raster.drawField(held, t);
        if (!blind) c.raster.drawLabels(held, t);
    }
    g.raster(c.raster);

    if (blind) {  // fog over the pot with question marks where the drops are
        const float fog = 0.86f * (1.f - peek);
        g.rect(potX(0), potY(), potW_, potH_, withAlpha(mix(t.sky[1], t.ink, 0.5f), int(255.f * fog)));
        for (const Drop& d : w_[0].drops)
            g.text("?", potX(0) + d.x, potY() + d.y + std::sin(simT_ * 2.f + d.x) * 1.5f, 2, withAlpha(t.text, int(220.f * fog)), Align::Center);
        if (peek > 0.5f) c.raster.drawLabels(labels, t), g.raster(c.raster);
    }
    if (flash_ > 0.f) g.rect(x, y, kW, kH, withAlpha(t.text, int(70.f * flash_)));
    drawDeco(c, x, y);
    fx_ = x;
    drawFx(c);
    // frame and the fade that hides the loop cut
    g.border(x - 1, y - 1, kW + 2, kH + 2, 1, t.ink);
    const float f = std::max(1.f - sstep(0.f, kFade, simT_), sstep(loop_ - kFade, loop_, simT_));
    if (f > 0.f) g.rect(x, y, kW, kH, withAlpha(t.ink, int(255.f * f)));
}

void ModePreview::draw(Ctx& c, ModeId mode, float x, float y, float dt) {
    if (!ready_ || mode != mode_) reset(c, mode);
    // reduce motion: show a still frame from the middle of the clip
    if (c.save.reduceMotion && simT_ >= loop_ * 0.55f) dt = 0.f;
    acc_ += std::min(dt, 0.05f);
    while (acc_ >= cfg::kStep) {
        acc_ -= cfg::kStep;
        simT_ += cfg::kStep;
        if (simT_ >= loop_) {
            reset(c, mode);
            break;
        }
        tick(c);
        for (Spark& s : sparks_) {
            s.x += s.vx * cfg::kStep;
            s.y += s.vy * cfg::kStep;
            s.vy += 140.f * cfg::kStep;
            s.life -= cfg::kStep;
        }
        for (Ring& r : rings_) r.age += cfg::kStep;
        for (Floater& f : floaters_) f.life -= cfg::kStep;
        sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(), [](const Spark& s) { return s.life <= 0.f; }), sparks_.end());
        rings_.erase(std::remove_if(rings_.begin(), rings_.end(), [](const Ring& r) { return r.age > 0.5f; }), rings_.end());
        floaters_.erase(std::remove_if(floaters_.begin(), floaters_.end(), [](const Floater& f) { return f.life <= 0.f; }), floaters_.end());
        shake_ = std::max(0.f, shake_ - cfg::kStep * 3.f);
        flash_ = std::max(0.f, flash_ - cfg::kStep * 2.5f);
    }
    drawScene(c, x, y);
}

}  // namespace liq
