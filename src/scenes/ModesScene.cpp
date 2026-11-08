// Mode selection: a scrollable board of animated tiles (Battle, Puzzle, Classic and Zen up front).

#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <string>

#include "game/Modifier.hpp"
#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "core/Util.hpp"
#include "game/Mode.hpp"
#include "game/World.hpp"
#include "scenes/Scenes.hpp"

namespace liq {

namespace {

constexpr float kHeaderH = 44.f;

struct Tile {
    ModeId mode;
    float x, y, w, h;
    bool big;  // shows the description line
};

// Layout in content coordinates (before scrolling), following the design sketch: a wide Battle tile, then a tall Puzzle
// tile beside stacked Classic and Zen, then the remaining modes in a two-column grid.
const Tile kTiles[] = {
    {ModeId::Battle, 14, 54, 242, 104, true},
    {ModeId::Puzzle, 14, 166, 94, 148, true},
    {ModeId::Classic, 116, 166, 140, 70, true},
    {ModeId::Zen, 116, 244, 140, 70, true},
    {ModeId::Timed, 14, 346, 119, 86, false},
    {ModeId::Target, 137, 346, 119, 86, false},
    {ModeId::Daily, 14, 440, 119, 86, false},
    {ModeId::Weekly, 137, 440, 119, 86, false},
    {ModeId::Mirror, 14, 552, 119, 100, false},
    {ModeId::BigBox, 137, 552, 119, 100, false},
    {ModeId::Madhouse, 14, 660, 119, 100, false},
    {ModeId::Blind, 137, 660, 119, 100, false},
};
constexpr int kTileCount = int(sizeof(kTiles) / sizeof(kTiles[0]));
constexpr float kLabelY = 328.f, kLabel2Y = 534.f, kModY = 776.f, kContentEnd = 816.f;

const char* nameKey(ModeId m) {
    static const char* k[kModeCount] = {"mode.classic", "mode.timed", "mode.target", "mode.daily", "mode.zen",
                                        "mode.rain",    "mode.tide",  "mode.weekly", "mode.puzzle", "mode.battle",
                                        "mode.mirror",  "mode.bigbox", "mode.madhouse", "mode.blind"};
    return k[int(m)];
}
const char* descKey(ModeId m) {
    static const char* k[kModeCount] = {"mode.classic.desc", "mode.timed.desc", "mode.target.desc", "mode.daily.desc",
                                        "mode.zen.desc",     "mode.rain.desc",  "mode.tide.desc",   "mode.weekly.desc",
                                        "mode.puzzle.desc",  "mode.battle.desc", "mode.mirror.desc", "mode.bigbox.desc", "mode.madhouse.desc",
                                        "mode.blind.desc"};
    return k[int(m)];
}

void line(Gfx& g, float x0, float y0, float x1, float y1, Color col) {
    const int n = std::max(1, int(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0))));
    for (int i = 0; i <= n; ++i) {
        const float u = float(i) / float(n);
        g.rect(std::floor(x0 + (x1 - x0) * u), std::floor(y0 + (y1 - y0) * u), 1, 1, col);
    }
}

// ---- Animated pictures ----------------------------------------------------------------------------------------------
// Every picture is a pure function of a looping phase built from sin/cos/smoothstep and grows or fades in and out from
// nothing, so when the loop wraps the first frame equals the last one: it never looks like "finished, start again".
constexpr float kTau = 6.2831853f;
float sstep(float e0, float e1, float x) {
    const float u = clampv((x - e0) / (e1 - e0), 0.f, 1.f);
    return u * u * (3.f - 2.f * u);
}
float pulse(float p) { return 0.5f - 0.5f * std::cos(p * kTau); }  // 0 at p = 0 and 1, 1 at p = 0.5
// Sprite whose size follows `scale` (0 = not there); the field draw fuses neighbours like in the game.
void put(std::vector<BlobSprite>& sp, float x, float y, int lvl, float scale, float wob, float ph) {
    if (scale < 0.04f) return;
    BlobSprite b = plainSprite(x, y, lvl, wob, ph);
    b.ra *= scale;
    b.rb *= scale;
    sp.push_back(b);
}

void drawIcon(Ctx& c, ModeId m, float cx, float cy, std::vector<BlobSprite>& sp) {
    Gfx& g = c.gfx;
    const Theme& t = *c.theme;
    const float T = c.save.reduceMotion ? 0.f : c.time;
    const float wob = 0.06f + 0.03f * std::sin(T * 2.f);
    switch (m) {
        case ModeId::Battle: {  // two drops shoving each other, sparks on every clash
            const float push = std::sin(T * 2.4f);
            const float r1 = World::radiusFor(1), r2 = World::radiusFor(2);
            const float gap = 24.f + 12.f * (1.f - push) * 0.5f;
            const float total = r1 + r2 + gap;
            const float x1 = cx - total / 2.f + r1 + push * 3.f, x2 = cx + total / 2.f - r2 + push * 3.f;
            const float hit = sstep(0.55f, 1.f, push);
            sp.push_back(plainSprite(x1, cy + 4.f, 1, wob + 0.06f * hit, T));
            sp.push_back(plainSprite(x2, cy + 4.f, 2, wob + 0.06f * hit, T + 1.f));
            if (hit > 0.02f)
                for (int i = 0; i < 4; ++i) {
                    const float a = float(i) * 1.5708f + 0.785f, rr = 6.f + 5.f * hit;
                    g.rect(cx + 2.f + std::cos(a) * rr, cy - 14.f + std::sin(a) * rr, 2, 2, withAlpha(t.accent, static_cast<Uint8>(std::clamp(255.f * hit, 0.f, 255.f))));
                }
            break;
        }
        case ModeId::Puzzle: {  // three drops (2, 4, 16) wandering
            const int lv[3] = {1, 2, 4};
            for (int i = 0; i < 3; ++i) {
                const float ph = float(i) * 2.1f;
                put(sp, cx + std::sin(T * 0.9f + ph) * 13.f, cy - 46.f + float(i) * 46.f + std::sin(T * 1.4f + ph * 1.3f) * 11.f, lv[i], 1.f, wob, T * 0.5f + ph);
            }
            break;
        }
        case ModeId::Zen: {  // one calm drop, ripples that fade in and out
            for (int i = 0; i < 3; ++i) {
                const float ph = std::fmod(T * 0.2f + float(i) / 3.f, 1.f);
                g.ring(cx, cy + 2.f, 12 + int(ph * 30.f), withAlpha(t.blob[0], static_cast<Uint8>(std::clamp(120.f * std::sin(ph * 3.1415927f), 0.f, 255.f))));
            }
            put(sp, cx, cy + 2.f + std::sin(T * 0.8f) * 2.f, 0, 1.f, 0.04f + 0.03f * std::sin(T * 0.8f), T * 0.3f);
            break;
        }
        case ModeId::Classic: {  // two drops glide together, fuse, and ease apart again
            const float k = pulse(std::fmod(T / 3.6f, 1.f));  // 0 apart .. 1 fused
            const float d = 2.f + 17.f * (1.f - k) * (1.f - k);
            const float jig = 0.16f * sstep(0.7f, 1.f, k) * std::cos(T * 9.f);
            put(sp, cx - d, cy + 3.f, 0, 1.f, wob + jig, T);
            put(sp, cx + d, cy + 3.f, 0, 1.f, wob + jig, T + 2.f);
            break;
        }
        case ModeId::Timed: {  // a clock; the hour hand and the sweeping hand never stop
            const Color face = mix(t.panelHi, t.text, 0.1f);
            g.disc(cx, cy, 17, t.ink);
            g.disc(cx, cy, 15, face);
            for (int i = 0; i < 12; ++i) {
                const float a = float(i) * 0.5236f;
                g.rect(std::floor(cx + std::sin(a) * 12.f), std::floor(cy - std::cos(a) * 12.f), 1, 1, t.ink);
            }
            const float a = T * 1.6f, b = T * 0.13f;
            line(g, cx, cy, cx + std::sin(a) * 12.f, cy - std::cos(a) * 12.f, t.danger);
            line(g, cx, cy, cx + std::sin(b) * 8.f, cy - std::cos(b) * 8.f, t.ink);
            g.rect(cx - 1, cy - 1, 2, 2, t.ink);
            g.rect(cx - 2, cy - 21, 4, 3, t.ink);
            break;
        }
        case ModeId::Target: {  // a drop grows at the top, falls in, splashes and melts into the bullseye
            const float p = std::fmod(T / 2.6f, 1.f);
            g.ring(cx, cy + 6.f, 17, t.text);
            g.ring(cx, cy + 6.f, 11, t.dim);
            g.ring(cx, cy + 6.f, 5 + int(pulse(std::fmod(T / 1.3f, 1.f)) * 2.f), t.accent);
            const float fall = sstep(0.18f, 0.55f, p);
            const float y = cy - 24.f + fall * fall * 30.f;
            const float scale = sstep(0.f, 0.18f, p) * (1.f - sstep(0.78f, 1.f, p));
            const float sq = 0.14f * sstep(0.55f, 0.6f, p) * std::exp(-(p - 0.55f) * 9.f) * std::cos((p - 0.55f) * 40.f);
            put(sp, cx, y, 0, scale, sq + 0.02f, T);
            break;
        }
        case ModeId::Daily:
        case ModeId::Weekly: {  // calendar page
            const bool week = m == ModeId::Weekly;
            const float w = 34.f, h = 32.f, x = cx - w / 2.f, y = cy - h / 2.f + 2.f;
            g.rect(x, y, w, h, mix(t.panelHi, t.text, 0.12f));
            g.border(x, y, w, h, 1, t.ink);
            g.rect(x + 1, y + 1, w - 2, 8, t.danger);
            g.rect(x + 7, y - 3, 2, 6, t.ink);
            g.rect(x + w - 9, y - 3, 2, 6, t.ink);
            const int on = week ? int(T * 1.6f) % 7 : 5;
            int n = 0;
            for (int r = 0; r < 3; ++r)
                for (int q = 0; q < 4; ++q, ++n) {
                    if (week && n >= 7) break;
                    const bool hi = n == on;
                    g.rect(x + 4.f + float(q) * 7.f, y + 13.f + float(r) * 6.f, hi ? 5.f : 4.f, hi ? 5.f : 4.f, hi ? t.accent : mix(t.dim, t.ink, 0.3f));
                }
            if (!week) {  // a sparkle that swells and fades on the day of the challenge
                const float s = pulse(std::fmod(T / 1.8f, 1.f)), sx = x + 13.f, sy = y + 9.f;
                const Uint8 a = static_cast<Uint8>(std::clamp(255.f * s, 0.f, 255.f)); const int l = 1 + int(s * 3.f);
                g.rect(sx - l, sy, 2 * l + 1, 1, withAlpha(t.accent, a));
                g.rect(sx, sy - l, 1, 2 * l + 1, withAlpha(t.accent, a));
            }
            break;
        }
        case ModeId::Mirror: {  // a drop and its reflection across a shimmering axis
            const float s = std::sin(T * 1.3f), y = cy + std::sin(T * 2.1f) * 6.f;
            const float dx = 15.f + 8.f * s;
            for (int i = -22; i <= 22; i += 4) g.rect(cx, cy + float(i), 1, 2, withAlpha(t.text, static_cast<Uint8>(std::clamp(70 + int(60.f * pulse(std::fmod(T / 2.f + float(i + 22) / 90.f, 1.f))), 0, 255))));
            put(sp, cx - dx, y, 1, 1.f, wob, T);
            put(sp, cx + dx, y, 1, 1.f, wob, -T);
            const float rr = 1.f + 0.25f * std::sin(T * 1.3f);
            g.ring(cx - dx, y + 14.f, int(7.f * rr), withAlpha(t.text, 45));
            g.ring(cx + dx, y + 14.f, int(7.f * rr), withAlpha(t.text, 45));
            break;
        }
        case ModeId::BigBox: {  // a wide container, four players' drops and a camera frame panning along
            const float w = 64.f, h = 36.f, x0 = cx - w / 2.f, top = cy - h / 2.f;
            g.rect(x0, top, w, h, withAlpha(t.ink, 110));
            const Color wc = mix(t.blob[6], t.sky[1], 0.4f);
            for (int x = 0; x < int(w); ++x) {
                const float wy = top + h - 9.f + std::sin(T * 2.f + float(x) * 0.3f) * 1.2f;
                g.rect(x0 + float(x), wy, 1, top + h - wy, wc);
            }
            g.border(x0 - 1, top - 1, w + 2, h + 2, 1, t.ink);
            for (int i = 0; i < 4; ++i) {  // players' drops bobbing in the water
                const float px = x0 + 9.f + float(i) * 15.f;
                put(sp, px, top + h - 11.f + std::sin(T * 1.7f + float(i) * 1.6f) * 2.f, i, 0.45f, 0.05f, T + float(i));
            }
            const float fx = x0 + (w - 26.f) * (0.5f + 0.5f * std::sin(T * 1.1f));  // camera frame
            g.border(fx, top + 2, 26, h - 4, 1, t.accent);
            g.rect(fx, top + 2, 3, 1, t.text);
            g.rect(fx + 23, top + h - 3, 3, 1, t.text);
            break;
        }
        case ModeId::Madhouse: {  // drops that keep changing number, size and colour (every swap hides at size 0)
            const int seq[6] = {0, 3, 1, 5, 2, 4};
            for (int i = 0; i < 3; ++i) {
                const float phi = T * 0.9f + float(i) * 1.05f;
                const float sa = std::cos(phi) * std::cos(phi), sb = 1.f - sa;
                const int ka = int(std::floor((phi + 1.5707963f) / 3.1415927f)), kb = int(std::floor(phi / 3.1415927f));
                const float x = cx + float(i - 1) * 24.f, y = cy + std::sin(T * 2.3f + float(i) * 2.f) * 9.f;
                put(sp, x, y, seq[((ka + i * 2) % 6 + 6) % 6], sa * 0.42f, wob + 0.05f, T + float(i));
                put(sp, x, y, seq[((kb + i * 2 + 3) % 6 + 6) % 6], sb * 0.42f, wob + 0.05f, T + float(i));
            }
            break;
        }
        case ModeId::Blind: {  // an eye that opens and closes over a drop of question marks
            const float lid = 0.15f + 0.85f * std::pow(pulse(std::fmod(T / 3.4f, 1.f)), 0.5f);
            const float ew = 22.f, eh = 11.f * lid;
            g.disc(cx, cy, 3, t.dim);
            for (int x = -int(ew); x <= int(ew); ++x) {
                const float u = float(x) / ew, hh = eh * std::sqrt(std::max(0.f, 1.f - u * u));
                g.rect(cx + float(x), cy - hh, 1, 2.f * hh + 1.f, mix(t.panelHi, t.text, 0.55f));
                g.rect(cx + float(x), cy - hh - 1.f, 1, 1, t.ink);
                g.rect(cx + float(x), cy + hh + 1.f, 1, 1, t.ink);
            }
            const float px = cx + std::sin(T * 0.9f) * 6.f;
            g.disc(px, cy, std::max(1.f, 5.f * lid), t.ink);
            g.disc(px, cy, std::max(0.f, 3.f * lid), t.accent);
            g.text("?", cx, cy - 25.f + std::sin(T * 2.f) * 2.f, 2, withAlpha(t.text, static_cast<Uint8>(std::clamp(120.f + 100.f * std::sin(T * 2.f), 0.f, 255.f))), Align::Center);
            break;
        }
        default: break;
    }
}

class ModesScene final : public Scene {
public:
    ModesScene() {
        if (const char* e = std::getenv("LIQ_MODES_SCROLL")) scroll_ = float(std::atof(e));  // dev: screenshots
    }

    void frame(Ctx& c, float dt) override {
        age_ += dt;
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        Input& in = c.in;
        const bool rtl = L.rtl();
        const float maxScroll = std::max(0.f, kContentEnd - float(cfg::kH));

        // ---- scrolling: drag, wheel, keys, inertia -------------------------------------------------------------
        if (in.pressed && in.my > kHeaderH) {
            pressY_ = in.my;
            pressScroll_ = scroll_;
            dragging_ = false;
            tracking_ = true;
            vel_ = 0.f;
            pressTile_ = tileAt(c, in.mx, in.my);
            keyNav_ = false;
        }
        if (tracking_ && in.down) {
            const float d = in.my - pressY_;
            if (!dragging_ && std::fabs(d) > 7.f) dragging_ = true;
            if (dragging_) {
                const float target = clampv(pressScroll_ - d, -24.f, maxScroll + 24.f);
                vel_ = (target - scroll_) / std::max(dt, 1.f / 240.f);
                scroll_ = target;
            }
        }
        bool tapped = false;
        if (in.released && tracking_) {
            tracking_ = false;
            if (!dragging_ && pressTile_ >= 0 && tileAt(c, in.mx, in.my) == pressTile_ && in.my > kHeaderH) tapped = true;
            if (!dragging_) vel_ = 0.f;
        }
        if (!tracking_ || !in.down) {
            scroll_ += vel_ * dt;
            vel_ *= std::pow(0.02f, dt);  // glide to a stop
            if (std::fabs(vel_) < 4.f) vel_ = 0.f;
            if (scroll_ < 0.f) scroll_ += (0.f - scroll_) * std::min(1.f, dt * 14.f);
            if (scroll_ > maxScroll) scroll_ += (maxScroll - scroll_) * std::min(1.f, dt * 14.f);
        }
        if (in.wheel != 0.f) scroll_ = clampv(scroll_ - in.wheel * 48.f, 0.f, maxScroll);
        if (in.dn || in.up) {
            keyNav_ = true;
            focus_ = (focus_ + (in.dn ? 1 : kTileCount - 1)) % kTileCount;
            const Tile& f = kTiles[focus_];
            if (f.y - scroll_ < kHeaderH + 6.f) scroll_ = f.y - kHeaderH - 6.f;
            if (f.y + f.h - scroll_ > float(cfg::kH) - 6.f) scroll_ = f.y + f.h - float(cfg::kH) + 6.f;
            scroll_ = clampv(scroll_, 0.f, maxScroll);
        }
        bool confirm = tapped ? true : false;
        int open = tapped ? pressTile_ : -1;
        if (in.confirm && keyNav_) {
            confirm = true;
            open = focus_;
        }

        // ---- draw -----------------------------------------------------------------------------------------------
        g.drawBackdrop(t, c.save.reduceMotion ? 0.f : c.time);
        std::vector<BlobSprite> sp;
        std::vector<std::function<void()>> post;  // tile text, drawn above the droplets
        const bool dragActive = dragging_;
        for (int i = 0; i < kTileCount; ++i) {
            const Tile& tl = kTiles[i];
            const float enter = sstep(0.f, 0.45f, age_ - float(i) * 0.04f);  // tiles settle in one after another
            const float x = ui.mx(tl.x, tl.w), y = tl.y - scroll_ + (1.f - enter) * 14.f;
            if (y + tl.h < kHeaderH - 2.f || y > float(cfg::kH)) continue;
            const bool held = tracking_ && in.down && !dragActive && pressTile_ == i && tileAt(c, in.mx, in.my) == i;
            const int ci = (int(tl.mode) * 5 + 3) % 12;
            const Color accent = t.blob[ci];
            Color fill = mix(t.panel, accent, 0.16f), hi = mix(t.panelHi, accent, 0.3f), lo = t.panelLo;
            if (held) {
                std::swap(hi, lo);
                fill = mix(fill, t.ink, 0.15f);
            }
            g.panel(x, y + (held ? 1.f : 0.f), tl.w, tl.h, fill, hi, lo, (keyNav_ && focus_ == i) ? t.accent : t.ink);
            if (keyNav_ && focus_ == i) g.border(x - 1, y - 1, tl.w + 2, tl.h + 2, 1, t.accent);
            g.rect(rtl ? x + tl.w - 5 : x + 2, y + 4, 3, tl.h - 8, accent);  // colour spine

            const float yy = y + (held ? 1.f : 0.f);
            const Align la = ui.start(), ra = rtl ? Align::Left : Align::Right;
            const float tx = rtl ? x + tl.w - 12 : x + 12;
            // icon placement per tile shape
            float icx = x + tl.w / 2.f, icy = yy + tl.h / 2.f - 6.f;
            if (tl.mode == ModeId::Battle) icx = rtl ? x + 60.f : x + tl.w - 62.f;
            if (tl.mode == ModeId::Classic || tl.mode == ModeId::Zen) icx = rtl ? x + 30.f : x + tl.w - 30.f;
            if (tl.mode == ModeId::Classic || tl.mode == ModeId::Zen) icy = yy + tl.h / 2.f;
            if (tl.mode == ModeId::Puzzle) icy = yy + 74.f;
            if (!tl.big) icy = yy + 36.f;
            if (!c.save.reduceMotion) {  // a soft glow behind the picture that breathes in the mode's colour
                const float br = pulse(std::fmod(c.time / 3.f + float(i) * 0.13f, 1.f));
                g.disc(icx, icy, 16.f + 5.f * br, withAlpha(accent, 22 + int(18.f * br)));
            }
            drawIcon(c, tl.mode, icx, icy, sp);

            post.push_back([=, &c, &g, &t, &L, &ui]() {
            if (tl.mode == ModeId::Battle) {
                g.text(L.tr(nameKey(tl.mode)), tx, yy + 18, 2, t.text, la, TextFx::Outline, t.ink);
                ui.paragraph(L.tr(descKey(tl.mode)), rtl ? x + tl.w - 12 - 108 : x + 12, yy + 42, 108, 1, t.dim, la);
                const std::string rec = std::to_string(c.save.battleWins) + " - " + std::to_string(c.save.battleLosses) + " - " +
                                        std::to_string(c.save.battleDraws);
                g.number(rec, tx, yy + tl.h - 14, 1, t.accent, la);
            } else if (tl.mode == ModeId::Puzzle) {
                g.text(L.tr(nameKey(tl.mode)), x + tl.w / 2.f, yy + 14, 1, t.text, Align::Center);
                g.number(std::to_string(c.save.puzzlesSolved()), x + tl.w / 2.f, yy + tl.h - 12, 1, t.accent, Align::Center);
            } else if (tl.big) {
                g.text(L.tr(nameKey(tl.mode)), tx, yy + 16, 1, t.text, la);
                ui.paragraph(L.tr(descKey(tl.mode)), rtl ? x + tl.w - 12 - 72 : x + 12, yy + 32, 72, 1, t.dim, la);
            } else {
                g.text(L.tr(nameKey(tl.mode)), x + tl.w / 2.f, yy + tl.h - 14, 1, t.text, Align::Center);
            }
            if (tl.mode >= ModeId::Mirror) {  // badge for the modes new in 1.8
                const float bw = 24.f;
                const float bx = rtl ? x + 6.f : x + tl.w - bw - 6.f;
                g.rect(bx, yy + 6, bw, 11, t.accent);
                g.border(bx, yy + 6, bw, 11, 1, t.ink);
                g.text(L.tr("hud.new"), bx + bw / 2.f, yy + 11.f, 1, t.ink, Align::Center);
            }
            int best = c.save.best[int(tl.mode)];
            if (tl.mode == ModeId::Daily) best = c.save.dailyDay == platform::dayNumber() ? c.save.dailyToday : 0;
            if (tl.mode == ModeId::Weekly) best = c.save.weeklyWeek == weekOf(platform::dayNumber()) ? c.save.weeklyBest : 0;
            if (best > 0 && tl.mode != ModeId::Puzzle && tl.mode != ModeId::Battle) {
                const std::string b = std::to_string(best);
                const float rx = rtl ? x + 8 : x + tl.w - 8;
                const float by = tl.big ? yy + tl.h - 12 : yy + tl.h - 28;
                g.number(b, rx, by, 1, t.accent, ra);
                const float iw = float(g.numberW(b, 1));
                g.icon(IconId::Crown, rtl ? rx + iw + 4 : rx - iw - 13, by - 3, t.accent);
            }
            });
        }

        // section label and modifier row
        {
            const float ly = kLabelY - scroll_;
            if (ly > kHeaderH && ly < float(cfg::kH)) {
                g.rect(14, ly, 242, 1, t.panelLo);
                g.text(L.tr("select.more"), float(cfg::kW) / 2.f, ly + 9, 1, t.dim, Align::Center);
            }
            const float ly2 = kLabel2Y - scroll_;
            if (ly2 > kHeaderH && ly2 < float(cfg::kH)) {
                g.rect(14, ly2, 242, 1, t.panelLo);
                g.text(L.tr("select.twists"), float(cfg::kW) / 2.f, ly2 + 9, 1, t.dim, Align::Center);
            }
            const float my = kModY - scroll_;
            if (my > kHeaderH + 2.f && my + 22.f < float(cfg::kH)) {
                                g.text(L.tr("mod.label"), rtl ? 256.f : 14.f, my + 10, 1, t.dim, ui.start());
                const float cw = 112.f, cx = ui.mx(132.f, cw);
                if (const int d = ui.stepper(cx, my, cw, 20, L.tr(modifierKey(c.save.modifier)))) {
                    c.save.modifier = (c.save.modifier + d + kModifierCount) % kModifierCount;
                    c.save.store();
                }
            }
        }

        if (!sp.empty()) {
            c.raster.clear();
            c.raster.drawField(sp, t);
            c.raster.drawLabels(sp, t);
            g.raster(c.raster);
        }
        for (auto& f : post) f();

        // ---- fixed header (drawn last so scrolled content slides under it) ----------------------------------
        g.rect(0, 0, float(cfg::kW), kHeaderH, mix(t.sky[0], t.ink, 0.55f));
        g.rect(0, kHeaderH - 1, float(cfg::kW), 1, t.panelLo);
        g.text(L.tr("select.title"), float(cfg::kW) / 2.f, 22, 2, t.accent, Align::Center, TextFx::Outline, t.ink);
        if (ui.button(L.tr("common.back"), ui.mx(8.f, 52.f), 10, 52, 24) || in.esc) c.go(SceneId::Menu);

        // scroll indicator
        if (maxScroll > 0.f) {
            const float trackH = float(cfg::kH) - kHeaderH - 12.f;
            const float bar = std::max(24.f, trackH * float(cfg::kH) / kContentEnd);
            const float by = kHeaderH + 6.f + (trackH - bar) * clampv(scroll_ / maxScroll, 0.f, 1.f);
            g.rect(rtl ? 2.f : float(cfg::kW) - 4.f, by, 2, bar, withAlpha(t.text, 90));
        }

        if (confirm && open >= 0) {
            c.audio.play(Sfx::Click);
            const ModeId m = kTiles[open].mode;
            c.mode = m;
            c.go(m == ModeId::Puzzle ? SceneId::Puzzle : m == ModeId::Battle ? SceneId::Battle : (m == ModeId::Mirror || m == ModeId::BigBox) ? SceneId::Party : SceneId::Game);
        }
    }

private:
    int tileAt(Ctx& c, float mx, float my) const {
        for (int i = 0; i < kTileCount; ++i) {
            const Tile& tl = kTiles[i];
            const float x = c.ui.mx(tl.x, tl.w), y = tl.y - scroll_;
            if (mx >= x && mx < x + tl.w && my >= y && my < y + tl.h) return i;
        }
        return -1;
    }

    float age_ = 0.f, scroll_ = 0.f, vel_ = 0.f, pressY_ = 0.f, pressScroll_ = 0.f;
    bool dragging_ = false, tracking_ = false, keyNav_ = false;
    int pressTile_ = -1, focus_ = 0;
};

}  // namespace

std::unique_ptr<Scene> makeModeSelectScene() { return std::make_unique<ModesScene>(); }

}  // namespace liq



