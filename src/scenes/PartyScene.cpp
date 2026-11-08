// Party modes (v1.8.0): Mirror (two narrow pots, one shared queue, the drops take turns) and Big Container (1-8 players
// in one wide pot; the camera follows every drop down and then returns). Self-contained: own physics worlds, HUD and input.

#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdio>
#include <optional>

#include "core/Config.hpp"
#include "core/Util.hpp"
#include "game/Liquid.hpp"
#include "game/World.hpp"
#include "scenes/Scenes.hpp"

namespace liq {
namespace {

bool hasEnv(const char* name) {
    return std::getenv(name) != nullptr;
}

std::string getEnv(const char* name) {
    const char* value = std::getenv(name);

    if (value == nullptr) {
        return {};
    }

    return std::string(value);
}

constexpr int kFieldR = 9, kColorR = 6;
constexpr float kIso = 1.1f;
constexpr float kMirrorW = 112.f, kMirrorGap = 124.f;
constexpr float kPotL = 24.f;
constexpr float kBoxBase = cfg::kFieldR - cfg::kFieldL;

float sm(float a, float b, float x) {
    const float u = clampv((x - a) / (b - a), 0.f, 1.f);
    return u * u * (3.f - 2.f * u);
}

struct FText {
    float x = 0.f;
    float y = 0.f;
    float t = 0.f;
    std::string s;
    bool big = false;
};

class PartyScene final : public Scene {
public:
    void frame(Ctx& c, float dt) override {
        if (!inited_) init(c);
        if (ph_ == Ph::Setup) setup(c);
        else play(c, dt);
    }

private:
    enum class Ph { Setup, Play, Over };

    // ---- setup ----
    bool inited_ = false;
    Ph ph_ = Ph::Setup;
    ModeId mode_ = ModeId::Mirror;
    int players_ = 2, sizeSel_ = 0;  // sizeSel_ 0 = automatic, 1..7 = 1.0x .. 2.5x
    bool mirror() const { return mode_ == ModeId::Mirror; }
    float mult() const { return sizeSel_ == 0 ? std::min(2.f, 1.f + 0.2f * float(players_ - 1)) : 1.f + 0.25f * float(sizeSel_ - 1); }
    bool aiming_ = false, following_ = false, best_ = false, demo_ = false;

    // ---- play ----
    World w_[2];
    int nw_ = 1;
    float boxW_ = kBoxBase, boxH_ = 348.f, floorY_ = cfg::kFloor, deadY_ = cfg::kDeadY + 22.f;
    Rng rng_;
    int cur_ = 0, next_ = 0, drops_ = 0, score_ = 0, last_ = 0, scores_[8] = {};
    float aim_ = 0.5f, cooldown_ = 0.f, acc_ = 0.f, camX_ = 0.f, camY_ = 0.f, followT_ = 0.f, calmT_ = 0.f, fx_ = 0.f, fy_ = 0.f;
    float demoT_ = 0.f, overT_ = 0.f;
    int target_ = 0;  // pot that receives the current drop (Mirror)
    std::vector<FText> texts_;

    int turn() const { return mirror() ? 0 : drops_ % std::max(1, players_); }

    void init(Ctx& c) {
        inited_ = true;
        mode_ = c.mode == ModeId::BigBox ? ModeId::BigBox : ModeId::Mirror;

        const std::string partyPlayers = getEnv("LIQ_PARTY_PLAYERS");

        if (!partyPlayers.empty()) {
            players_ = clampv(std::atoi(partyPlayers.c_str()), 1, 8);
        }

        demo_ = hasEnv("LIQ_PARTY_DEMO");

        if (demo_) {
            begin(c);
        }
    }

    int roll() {
        const int r = rng_.range(0, 99);
        static const int wt[5][6] = {{38, 70, 90, 100, 100, 100}, {28, 56, 78, 93, 100, 100}, {20, 42, 66, 86, 100, 100},
                                     {14, 32, 54, 78, 100, 100}, {10, 24, 44, 68, 88, 100}};
        const int tier = std::min(4, drops_ / 12 + (mirror() ? 0 : (players_ > 1 ? 1 : 0) + (mult() > 1.4f ? 1 : 0)));
        int l = 0;
        while (l < 5 && r >= wt[tier][l]) ++l;
        return l;
    }

    void begin(Ctx& c) {
        ph_ = Ph::Play;
        nw_ = mirror() ? 2 : 1;
        const float m = mirror() ? 1.f : mult();
        const float hm = 1.f + 0.25f * (m - 1.f);
        boxW_ = mirror() ? kMirrorW : kBoxBase * m;
        boxH_ = 348.f * (mirror() ? 1.f : hm);
        floorY_ = float(cfg::kTankTop) + boxH_;
        for (int i = 0; i < nw_; ++i) {
            World& w = w_[i];
            w.clear();
            w.fieldL = kPotL;
            w.fieldR = kPotL + boxW_;
            w.floorY = floorY_;
            w.worldW = kPotL + boxW_ + 40.f;
            w.worldH = floorY_ + 40.f;
            applyLiquid(w, c.save.liquid);
            w.substeps = (!mirror() && m > 1.6f) ? 1 : c.save.quality + 1;
        }
        rng_.seed(uint32_t(SDL_GetTicks()) * 2246822519u + 11u);
        drops_ = score_ = last_ = 0;
        for (int& s : scores_) s = 0;
        cur_ = roll();
        next_ = roll();
        aim_ = 0.5f;
        cooldown_ = 0.f;
        acc_ = 0.f;
        camX_ = camXRest();
        camY_ = 0.f;
        following_ = false;
        best_ = false;
        overT_ = 0.f;
        texts_.clear();
        target_ = 0;
    }

    // ---- geometry helpers ----
    float radHeld() const { return World::radiusFor(cur_) * 0.9f; }
    float aimWorldX(int pot) const {
        const float rad = radHeld(), lo = kPotL + rad + 1.f, hi = kPotL + boxW_ - rad - 1.f;
        const float u = (mirror() && pot == 1) ? 1.f - aim_ : aim_;
        return lo + (hi - lo) * u;
    }
    float camXRest() const {
        const float potCx = mirror() ? (kPotL + kMirrorGap + kMirrorW + kPotL) / 2.f : kPotL + boxW_ / 2.f;
        const float total = mirror() ? kMirrorGap + kMirrorW + 12.f : boxW_ + 24.f;
        if (total <= float(cfg::kW)) return potCx - float(cfg::kW) / 2.f;
        return clampv(aimWorldX(0) - float(cfg::kW) / 2.f, kPotL - 14.f, kPotL + boxW_ + 14.f - float(cfg::kW));
    }
    float camYMax() const { return std::max(0.f, floorY_ + 22.f - float(cfg::kH)); }

    void dropNow(Ctx& c) {
        if (cooldown_ > 0.f) return;
        const int pot = mirror() ? (drops_ % 2) : 0;
        World& w = w_[pot];
        const float x = aimWorldX(pot);
        w.spawn(x, cfg::kSpawnY, cur_);
        last_ = turn();
        ++drops_;
        cur_ = next_;
        next_ = roll();
        cooldown_ = cfg::kDropCooldown;
        c.audio.play(Sfx::Drop, 0.5f);
        target_ = mirror() ? (drops_ % 2) : 0;
        if (!mirror()) {
            following_ = true;
            followT_ = 0.f;
            calmT_ = 0.f;
            fx_ = x;
            fy_ = cfg::kSpawnY;
        }
    }

    void onMerge(Ctx& c, const MergeEvent& e, int pot) {
        const int v = (1 << e.level) * e.chain;
        score_ += v;
        scores_[last_] += v;
        c.audio.play(Sfx::Merge, 0.5f, e.level, e.chain);
        const float ox = mirror() ? (pot ? kMirrorGap : 0.f) : 0.f;
        texts_.push_back({e.x + ox, e.y - World::radiusFor(e.level) - 4.f, 0.9f, "+" + std::to_string(v), e.chain >= 3});
    }

    void finish(Ctx& c) {
        ph_ = Ph::Over;
        overT_ = 0.f;
        const int top = mirror() || players_ == 1 ? score_ : *std::max_element(scores_, scores_ + players_);
        if (top > c.save.best[int(mode_)]) {
            c.save.best[int(mode_)] = top;
            best_ = true;
            c.save.store();
        }
        c.audio.play(Sfx::Fail);
    }

    // ---- simulation ----
    void simulate(Ctx& c, float dt) {
        acc_ = std::min(acc_ + dt, cfg::kStep * 3.f);
        while (acc_ >= cfg::kStep) {
            acc_ -= cfg::kStep;
            bool out = false;
            for (int i = 0; i < nw_; ++i) {
                World& w = w_[i];
                w.step(cfg::kStep, cfg::kGravity);
                for (const auto& e : w.events) onMerge(c, e, i);
                w.events.clear();
                w.impacts.clear();
                w.updateOverflow(deadY_, cfg::kStep);
                if (w.overflow() > cfg::kOverflowSeconds) out = true;
            }
            if (out) {
                finish(c);
                return;
            }
        }
    }

    // The camera of Big Container: it tracks the aim while you hold a drop, follows the dropped one down, then returns.
    void updateCamera(float dt) {
        if (mirror()) {
            camX_ = camXRest();
            camY_ = 0.f;
            return;
        }
        float tx = camXRest(), ty = 0.f;
        if (following_) {
            followT_ += dt;
            const World& w = w_[0];
            float best = 1e9f, vy = 0.f, bx = fx_, by = fy_;
            for (const Drop& d : w.drops) {  // the drop nearest to where we last saw ours
                const float dd = (d.x - fx_) * (d.x - fx_) + (d.y - fy_) * (d.y - fy_);
                if (dd < best) {
                    best = dd;
                    bx = d.x;
                    by = d.y;
                    vy = std::fabs(d.vy) + std::fabs(d.vx);
                }
            }
            fx_ = bx;
            fy_ = by;
            calmT_ = vy < 25.f ? calmT_ + dt : 0.f;
            if ((followT_ > 0.6f && calmT_ > 0.35f) || followT_ > 4.f) following_ = false;
            const float total = boxW_ + 24.f;
            if (total > float(cfg::kW)) tx = clampv(fx_ - float(cfg::kW) / 2.f, kPotL - 14.f, kPotL + boxW_ + 14.f - float(cfg::kW));
            ty = clampv(fy_ - 250.f, 0.f, camYMax());
        }
        const float k = following_ ? 6.f : 3.f;  // quick while chasing, a gentle ease on the way back
        camX_ += (tx - camX_) * std::min(1.f, dt * k);
        camY_ += (ty - camY_) * std::min(1.f, dt * k);
    }

    // ---- frame ----
    void setup(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        g.drawBackdrop(t, c.save.reduceMotion ? 0.f : c.time);
        g.text(L.tr(mirror() ? "mode.mirror" : "mode.bigbox"), cfg::kW / 2.f, 40, 2, t.accent, Align::Center, TextFx::Outline, t.ink);
        ui.panel(15, 66, 240, 86);
        ui.paragraph(L.tr(mirror() ? "mode.mirror.desc" : "mode.bigbox.desc"), 25, 78, 220, 1, t.text, Align::Center);
        float y = 176.f;
        if (!mirror()) {
            g.text(L.tr("party.players"), 22, y + 10, 1, t.dim);
            if (const int d = ui.stepper(120, y, 130, 22, std::to_string(players_), true)) players_ = clampv(players_ + d, 1, 8);
            y += 34.f;
            g.text(L.tr("party.size"), 22, y + 10, 1, t.dim);
            const std::string sv = sizeSel_ == 0 ? L.tr("party.auto") + " " + fmtMult(mult()) : fmtMult(mult());
            if (const int d = ui.stepper(120, y, 130, 22, sv, sizeSel_ != 0)) sizeSel_ = (sizeSel_ + d + 8) % 8;
            y += 34.f;
            if (mult() >= 2.f) ui.paragraph(L.tr("party.slow"), 20, y, 230, 1, t.dim, Align::Center);
            y += 34.f;
        } else {
            ui.paragraph(L.tr("party.mirror.how"), 20, y, 230, 1, t.dim, Align::Center);
            y += 60.f;
        }
        const int best = c.save.best[int(mode_)];
        if (best > 0) g.text(L.tr("hud.best") + " " + std::to_string(best), cfg::kW / 2.f, y + 4, 1, t.accent, Align::Center);
        if (ui.button(L.tr("party.start"), 55, 372, 160, 30) || c.in.confirm) begin(c);
        if (ui.button(L.tr("common.back"), 55, 412, 160, 26) || c.in.esc) c.go(SceneId::ModeSelect);
    }

    static std::string fmtMult(float m) {
        char b[16];
        std::snprintf(b, sizeof b, "%.2gx", double(m));
        return b;
    }

    void play(Ctx& c, float dt) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        Input& in = c.in;
        if (ph_ == Ph::Play) {
            // input: dragging sets the aim across the whole container, keys nudge it, release / space drops
            if (in.pressed && in.my > 64.f) aiming_ = true;
            if (aiming_ && (in.down || in.released)) aim_ = clampv(in.mx / float(cfg::kW), 0.f, 1.f);
            const float span = std::max(60.f, boxW_);
            if (in.left) aim_ = clampv(aim_ - cfg::kAimSpeed * dt / span, 0.f, 1.f);
            if (in.right) aim_ = clampv(aim_ + cfg::kAimSpeed * dt / span, 0.f, 1.f);
            if (aiming_ && in.released) {
                aiming_ = false;
                dropNow(c);
            }
            if (in.drop || in.confirm || in.dn) dropNow(c);
            if (demo_ && cooldown_ <= 0.f) {  // dev: scripted drops for screenshots
                demoT_ += dt;
                aim_ = 0.5f + 0.45f * std::sin(demoT_ * 1.7f);
                if (demoT_ > 0.5f) {
                    demoT_ = 0.f;
                    dropNow(c);
                }
            }
            cooldown_ = std::max(0.f, cooldown_ - dt);
            simulate(c, dt);
            if (in.esc) c.go(SceneId::ModeSelect);
        } else {
            overT_ += dt;
        }
        updateCamera(dt);
        for (auto& f : texts_) f.t -= dt;
        texts_.erase(std::remove_if(texts_.begin(), texts_.end(), [](const FText& f) { return f.t <= 0.f; }), texts_.end());

        // ---- draw world (camera = integer pixel offset) ----
        g.drawBackdrop(t, c.save.reduceMotion ? 0.f : c.time);
        const int cx = int(std::lround(camX_)), cy = int(std::lround(camY_));
        g.setOffset(-cx, -cy);
        for (int i = 0; i < nw_; ++i) {
            const float ox = mirror() && i == 1 ? kMirrorGap : 0.f;
            drawPot(c, ox, i);
        }
        std::vector<BlobSprite> pile, labels, held;
        for (int i = 0; i < nw_; ++i) {
            const float ox = (mirror() && i == 1 ? kMirrorGap : 0.f) - float(cx), oy = -float(cy);
            const size_t p0 = pile.size(), l0 = labels.size();
            w_[i].particleSprites(1.f, 0.f, pile);
            w_[i].labelSprites(labels);
            for (size_t k = p0; k < pile.size(); ++k) {
                pile[k].x += ox;
                pile[k].y += oy;
            }
            for (size_t k = l0; k < labels.size(); ++k) {
                labels[k].x += ox;
                labels[k].y += oy;
            }
        }
        if (ph_ == Ph::Play) {
            const float grow = sm(0.f, 1.f, 1.f - cooldown_ / cfg::kDropCooldown);
            const int pot = mirror() ? (drops_ % 2) : 0;
            const float ox = (mirror() && pot == 1 ? kMirrorGap : 0.f) - float(cx);
            BlobSprite b = plainSprite(aimWorldX(pot) + ox, cfg::kSpawnY - float(cy), cur_, 0.06f, c.time);
            b.ra *= grow;
            b.rb *= grow;
            if (b.ra > 2.f) held.push_back(b);
        }
        c.raster.clear();
        c.raster.drawLiquid(pile, t, kFieldR, kColorR, kIso);
        c.raster.drawLabels(labels, t);
        if (!held.empty()) {
            c.raster.drawField(held, t);
            c.raster.drawLabels(held, t);
        }
        g.setOffset(0, 0);
        g.raster(c.raster);
        g.setOffset(-cx, -cy);
        // aim guide
        if (ph_ == Ph::Play && c.save.guide) {
            const int pot = mirror() ? (drops_ % 2) : 0;
            const float ox = mirror() && pot == 1 ? kMirrorGap : 0.f;
            const float gx = aimWorldX(pot) + ox, sy = w_[pot].surfaceY(aimWorldX(pot), radHeld());
            for (float y = cfg::kSpawnY + 24.f; y < sy - 4.f; y += 8.f) g.rect(std::floor(gx), std::floor(y), 1, 3, withAlpha(t.text, static_cast<Uint8>(70)));
        }
        for (const auto& f : texts_) {
            const float u = 1.f - f.t / 0.9f;
            g.number(f.s, f.x, f.y - u * 22.f, f.big ? 2 : 1, withAlpha(f.big ? t.accent : t.text,static_cast<Uint8>(std::clamp(static_cast<int>(255.f * (1.f - u * u)),0,255))), Align::Center, TextFx::Outline, t.ink);
        }
        g.setOffset(0, 0);
        if (!mirror()) drawGauge(c);
        drawHud(c);
        if (ph_ == Ph::Over) drawOver(c);
        (void)L;
        (void)ui;
    }

    void drawPot(Ctx& c, float ox, int idx) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        const float L = kPotL + ox, R = kPotL + boxW_ + ox, top = float(cfg::kTankTop), fl = floorY_;
        g.rect(L, top, R - L, fl - top, withAlpha(t.ink, 105));
        // overflow line
        const bool warn = w_[idx].overflow() > 0.2f;
        for (float x = L; x < R; x += 6.f)g.rect(static_cast<float>(std::floor(x)),static_cast<float>(std::floor(deadY_)),3.f, warn ? 2.f : 1.f, t.danger);
        // floor
        g.rect(L - 6, fl, R - L + 12, 10, t.frame);
        g.rect(L - 6, fl, R - L + 12, 1, t.ink);
        g.rect(L - 5, fl + 1, R - L + 10, 1, t.frameHi);
        g.rect(L - 5, fl + 7, R - L + 10, 2, t.frameLo);
        g.rect(L - 6, fl + 9, R - L + 12, 1, t.ink);
        auto wall = [&](float x) {
            const float bot = fl + 10.f;
            g.rect(x, top, 1, bot - top, t.ink);
            g.rect(x + 1, top, 1, bot - top - 1, t.frameHi);
            g.rect(x + 2, top, 2, bot - top - 1, t.frame);
            g.rect(x + 4, top, 1, bot - top - 1, t.frameLo);
            g.rect(x + 5, top, 1, bot - top, t.ink);
        };
        wall(L - 6);
        wall(R);
        for (float y = top + 16.f; y < fl - 6.f; y += 34.f) {
            g.rect(L - 4, y, 2, 2, t.frameLo);
            g.rect(R + 2, y, 2, 2, t.frameLo);
        }
    }

    // Depth gauge (Big Container): how full the pot is and which part of it the camera shows.
    void drawGauge(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        const float x = float(cfg::kW) - 5.f, y0 = 70.f, h = float(cfg::kH) - 80.f, top = float(cfg::kTankTop), tall = floorY_ - top;
        g.rect(x, y0, 3, h, withAlpha(t.ink, 150));
        float hi = floorY_;
        for (const Drop& d : w_[0].drops) hi = std::min(hi, d.top);
        const float fill = clampv((floorY_ - hi) / tall, 0.f, 1.f), lineF = (floorY_ - deadY_) / tall;
        g.rect(x, y0 + h * (1.f - fill), 3, h * fill, withAlpha(t.blob[6], 200));
        g.rect(x - 1, y0 + h * (1.f - lineF), 5, 1, t.danger);
        const float vy0 = clampv((camY_ + float(cfg::kTankTop) - top) / tall, 0.f, 1.f), vh = clampv(float(cfg::kH) / tall, 0.f, 1.f);
        g.border(x - 1, y0 + h * vy0, 5, std::max(4.f, h * vh), 1, t.accent);
    }

    void drawHud(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        g.rect(0, 0, float(cfg::kW), 62.f, mix(t.sky[0], t.ink, 0.6f));
        g.rect(0, 61, float(cfg::kW), 1, t.panelLo);
        // next drop
        ui.panel(8, 8, 44, 46);
        g.text(L.tr("hud.next"), 30, 15, 1, t.dim, Align::Center);
        if (ph_ == Ph::Play) {
            std::vector<BlobSprite> nx{plainSprite(30.f, 37.f, next_)};
            nx[0].ra = nx[0].rb = std::min(nx[0].ra, 12.f);
            c.raster.clear();
            c.raster.drawField(nx, t);
            c.raster.drawLabels(nx, t);
            g.raster(c.raster);
        }
        if (mirror() || players_ == 1) {
            g.text(L.tr("over.score"), 160, 16, 1, t.dim, Align::Center);
            g.number(std::to_string(score_), 160, 38, 2, t.text, Align::Center, TextFx::Outline, t.ink);
            const int b = c.save.best[int(mode_)];
            if (b > 0) g.text(L.tr("hud.best") + " " + std::to_string(b), 160, 54, 1, t.dim, Align::Center);
        } else {
            const int cols = players_ <= 4 ? players_ : 4, rows = players_ <= 4 ? 1 : 2;
            const float cw = 200.f / float(cols), ch = rows == 1 ? 40.f : 23.f;
            for (int i = 0; i < players_; ++i) {
                const float x = 58.f + float(i % cols) * cw, y = 8.f + float(i / cols) * (ch + 2.f);
                const bool me = ph_ == Ph::Play && i == turn();
                g.rect(x, y, cw - 2.f, ch, me ? mix(t.panelHi, t.blob[(i * 3) % 12], 0.5f) : t.panel);
                g.border(x, y, cw - 2.f, ch, 1, me ? t.accent : t.ink);
                g.disc(static_cast<float>(std::lround(x + 7.f)),static_cast<float>(std::lround(y + ch / 2.f)),3,t.blob[(i * 3) % 12]);
                g.text("P" + std::to_string(i + 1), x + 13.f, y + (rows == 1 ? 12.f : 8.f), 1, me ? t.text : t.dim);
                g.number(std::to_string(scores_[i]), x + cw - 6.f, y + ch - (rows == 1 ? 12.f : 8.f), 1, t.text, Align::Right);
            }
        }
    }

    void drawOver(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        const float u = sm(0.f, 0.4f, overT_);
        const Uint8 overlayAlpha = static_cast<Uint8>(std::clamp(static_cast<int>(150.f * u), 0, 255));
        g.rect(0.f, 0.f, float(cfg::kW), float(cfg::kH),withAlpha(t.ink, overlayAlpha));
        const float py = 150.f + (1.f - u) * 20.f;
        ui.panel(25, py, 220, 190);
        g.text(L.tr("party.over"), 135, py + 18, 2, t.accent, Align::Center, TextFx::Outline, t.ink);
        if (mirror() || players_ == 1) {
            g.number(std::to_string(score_), 135, py + 60, 2, t.text, Align::Center, TextFx::Outline, t.ink);
            if (best_) g.text(L.tr("over.newbest"), 135, py + 84, 1, t.accent, Align::Center);
        } else {
            int win = 0;
            for (int i = 1; i < players_; ++i)
                if (scores_[i] > scores_[win]) win = i;
            g.text(L.fmt("party.winner", "P" + std::to_string(win + 1)), 135, py + 48, 1, t.accent, Align::Center);
            const int n = std::min(players_, 8);
            for (int i = 0; i < n; ++i) {
                const float x = 40.f + float(i / 4) * 100.f, y = py + 66.f + float(i % 4) * 14.f;
                g.disc(static_cast<float>(std::lround(x + 3.f)),static_cast<float>(std::lround(y)),3,t.blob[(i * 3) % 12]);
                g.text("P" + std::to_string(i + 1), x + 10.f, y, 1, i == win ? t.accent : t.text);
                g.number(std::to_string(scores_[i]), x + 90.f, y, 1, t.text, Align::Right);
            }
        }
        if (ui.button(L.tr("over.restart"), 40, py + 128, 190, 24)) begin(c);
        if (ui.button(L.tr("common.back"), 40, py + 156, 190, 24) || c.in.esc) c.go(SceneId::ModeSelect);
    }
};

}  // namespace

std::unique_ptr<Scene> makePartyScene() { return std::make_unique<PartyScene>(); }

}  // namespace liq
