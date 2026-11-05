// Mode selection: a scrollable board of animated tiles (Battle, Puzzle, Classic and Zen up front).
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <string>

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
    {ModeId::Rain, 137, 440, 119, 86, false},
    {ModeId::Tide, 14, 534, 119, 86, false},
    {ModeId::Weekly, 137, 534, 119, 86, false},
};
constexpr int kTileCount = int(sizeof(kTiles) / sizeof(kTiles[0]));
constexpr float kLabelY = 328.f, kModY = 636.f, kContentEnd = 676.f;

const char* nameKey(ModeId m) {
    static const char* k[kModeCount] = {"mode.classic", "mode.timed", "mode.target", "mode.daily", "mode.zen",
                                        "mode.rain",    "mode.tide",  "mode.weekly", "mode.puzzle", "mode.battle"};
    return k[int(m)];
}
const char* descKey(ModeId m) {
    static const char* k[kModeCount] = {"mode.classic.desc", "mode.timed.desc", "mode.target.desc", "mode.daily.desc",
                                        "mode.zen.desc",     "mode.rain.desc",  "mode.tide.desc",   "mode.weekly.desc",
                                        "mode.puzzle.desc",  "mode.battle.desc"};
    return k[int(m)];
}

void line(Gfx& g, float x0, float y0, float x1, float y1, Color col) {
    const int n = std::max(1, int(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0))));
    for (int i = 0; i <= n; ++i) {
        const float u = float(i) / float(n);
        g.rect(std::floor(x0 + (x1 - x0) * u), std::floor(y0 + (y1 - y0) * u), 1, 1, col);
    }
}

// Animated picture for one mode, drawn around (cx, cy). Droplets go into `sp` (drawn in one pass later).
void drawIcon(Ctx& c, ModeId m, float cx, float cy, std::vector<BlobSprite>& sp) {
    Gfx& g = c.gfx;
    const Theme& t = *c.theme;
    const float T = c.save.reduceMotion ? 0.f : c.time;
    const float wob = 0.06f + 0.03f * std::sin(T * 2.f);
    switch (m) {
        case ModeId::Battle: {  // two drops shoving each other
            const float push = std::sin(T * 3.f);
            const float r1 = World::radiusFor(1), r2 = World::radiusFor(2);
            const float gap = 24.f + 12.f * (1.f - push) * 0.5f;
            const float total = r1 + r2 + gap;
            const float x1 = cx - total / 2.f + r1 + push * 3.f, x2 = cx + total / 2.f - r2 + push * 3.f;
            sp.push_back(plainSprite(x1, cy + 4.f, 1, wob + 0.05f * std::max(0.f, push), T));
            sp.push_back(plainSprite(x2, cy + 4.f, 2, wob + 0.05f * std::max(0.f, push), T + 1.f));
            if (push > 0.8f) {  // clash sparks
                const Color sc = t.accent;
                for (int i = 0; i < 4; ++i) {
                    const float a = float(i) * 1.5708f + 0.785f;
                    g.rect(cx + 2.f + std::cos(a) * 9.f, cy - 14.f + std::sin(a) * 9.f, 2, 2, sc);
                }
            }
            break;
        }
        case ModeId::Puzzle: {  // three drops (2, 4, 16) wandering up and down
            const int lv[3] = {1, 2, 4};
            for (int i = 0; i < 3; ++i) {
                const float ph = float(i) * 2.1f;
                const float x = cx + std::sin(T * 0.9f + ph) * 13.f;
                const float y = cy - 46.f + float(i) * 46.f + std::sin(T * 1.4f + ph * 1.3f) * 11.f;
                sp.push_back(plainSprite(x, y, lv[i], wob, T * 0.5f + ph));
            }
            break;
        }
        case ModeId::Zen: {  // one calm drop, slow ripples
            for (int i = 0; i < 3; ++i) {
                const float ph = std::fmod(T * 0.22f + float(i) / 3.f, 1.f);
                const int a = int(110.f * (1.f - ph));
                g.ring(cx, cy + 2.f, 12 + int(ph * 30.f), withAlpha(t.blob[0], a));
            }
            sp.push_back(plainSprite(cx, cy + 2.f + std::sin(T * 0.8f) * 2.f, 0, 0.04f + 0.03f * std::sin(T * 0.8f), T * 0.3f));
            break;
        }
        case ModeId::Classic: {  // two drops fuse into a bigger one, over and over
            const float p = std::fmod(T, 3.2f);
            if (p < 1.5f) {
                const float k = 1.f - p / 1.5f;
                const float d = 2.f + 17.f * k * k;
                sp.push_back(plainSprite(cx - d, cy + 3.f, 0, wob, T));
                sp.push_back(plainSprite(cx + d, cy + 3.f, 0, wob, T + 2.f));
            } else {
                const float q = p - 1.5f;
                sp.push_back(plainSprite(cx, cy + 2.f, 1, 0.16f * std::exp(-q * 3.f) * std::cos(q * 14.f) + 0.05f, T));
            }
            break;
        }
        case ModeId::Timed: {  // a clock with a sweeping hand
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
            g.rect(cx - 2, cy - 21, 4, 3, t.ink);  // crown button
            break;
        }
        case ModeId::Target: {  // bullseye with a drop landing in it
            const float pulse = 0.5f + 0.5f * std::sin(T * 3.f);
            g.ring(cx, cy + 6.f, 17, t.text);
            g.ring(cx, cy + 6.f, 11, t.dim);
            g.ring(cx, cy + 6.f, 5 + int(pulse * 2.f), t.accent);
            const float p = std::fmod(T, 2.2f);
            const float y = p < 1.f ? cy - 22.f + (p * p) * 28.f : cy + 6.f;
            const float sq = p < 1.f ? 0.f : 0.14f * std::exp(-(p - 1.f) * 5.f) * std::cos((p - 1.f) * 16.f);
            sp.push_back(plainSprite(cx, y, 0, sq + 0.02f, T));
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
            const int cols = 4, rows = 3;
            const int on = week ? int(T * 1.6f) % 7 : 5;
            int n = 0;
            for (int r = 0; r < rows; ++r)
                for (int q = 0; q < cols; ++q, ++n) {
                    const bool hi = week ? (n == on) : (n == on);
                    if (week && n >= 7) break;
                    const Color cc = hi ? t.accent : mix(t.dim, t.ink, 0.3f);
                    const float px = x + 4.f + float(q) * 7.f, py = y + 13.f + float(r) * 6.f;
                    g.rect(px, py, hi ? 5 : 4, hi ? 5 : 4, cc);
                }
            if (!week && std::fmod(T, 1.4f) < 0.7f) {  // twinkle on the day of the challenge
                const float sx = x + 4.f + 1.f * 7.f + 2.f, sy = y + 13.f + 1.f * 6.f - 10.f;
                g.rect(sx - 3, sy, 7, 1, t.accent);
                g.rect(sx, sy - 3, 1, 7, t.accent);
            }
            break;
        }
        case ModeId::Rain: {  // cloud and falling drops
            g.disc(cx - 9, cy - 12, 7, mix(t.dim, t.text, 0.4f));
            g.disc(cx + 2, cy - 16, 9, mix(t.dim, t.text, 0.5f));
            g.disc(cx + 12, cy - 11, 6, mix(t.dim, t.text, 0.4f));
            g.rect(cx - 16, cy - 12, 34, 6, mix(t.dim, t.text, 0.4f));
            for (int i = 0; i < 3; ++i) {
                const float p = std::fmod(T * 0.7f + float(i) / 3.f, 1.f);
                const float x = cx - 10.f + float(i) * 10.f;
                const float yy = cy - 3.f + p * 26.f;
                const Color dc = withAlpha(t.blob[6], int(255.f * std::min(1.f, (1.f - p) * 2.5f)));
                g.disc(x, yy, 2, dc);
                g.rect(x, yy - 5.f, 1, 3, dc);
            }
            break;
        }
        case ModeId::Tide: {  // water with a sinking danger line
            const float x0 = cx - 24.f, w = 48.f, top = cy - 18.f, h = 38.f;
            g.rect(x0, top, w, h, withAlpha(t.ink, 120));
            const float lineY = top + 5.f + (0.5f + 0.5f * std::sin(T * 0.9f)) * 9.f;
            for (int x = 0; x < int(w); x += 4) g.rect(x0 + float(x), lineY, 2, 1, t.danger);
            for (int x = 0; x < int(w); ++x) {
                const float wy = top + 22.f - (0.5f + 0.5f * std::sin(T * 0.9f)) * 4.f + std::sin(T * 2.f + float(x) * 0.35f) * 1.6f;
                g.rect(x0 + float(x), wy, 1, top + h - wy, mix(t.blob[6], t.sky[1], 0.35f));
            }
            g.border(x0, top, w, h, 1, t.ink);
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
            const float x = ui.mx(tl.x, tl.w), y = tl.y - scroll_;
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
            int best = c.save.best[int(tl.mode)];
            if (tl.mode == ModeId::Daily) best = c.save.dailyDay == platform::dayNumber() ? c.save.dailyToday : 0;
            if (tl.mode == ModeId::Weekly) best = c.save.weeklyWeek == weekOf(platform::dayNumber()) ? c.save.weeklyBest : 0;
            if (best > 0 && tl.mode != ModeId::Puzzle && tl.mode != ModeId::Battle) {
                const std::string b = std::to_string(best);
                const float rx = rtl ? x + 8 : x + tl.w - 8;
                const float by = tl.big ? yy + tl.h - 12 : yy + 11;
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
            const float my = kModY - scroll_;
            if (my > kHeaderH + 2.f && my + 22.f < float(cfg::kH)) {
                static const char* modKey[5] = {"mod.none", "mod.lowg", "mod.heavy", "mod.thick", "mod.bonus"};
                g.text(L.tr("mod.label"), rtl ? 256.f : 14.f, my + 10, 1, t.dim, ui.start());
                const float cw = 112.f, cx = ui.mx(132.f, cw);
                if (const int d = ui.stepper(cx, my, cw, 20, L.tr(modKey[c.save.modifier]))) {
                    c.save.modifier = (c.save.modifier + d + 5) % 5;
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
            c.go(m == ModeId::Puzzle ? SceneId::Puzzle : m == ModeId::Battle ? SceneId::Battle : SceneId::Game);
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

    float scroll_ = 0.f, vel_ = 0.f, pressY_ = 0.f, pressScroll_ = 0.f;
    bool dragging_ = false, tracking_ = false, keyNav_ = false;
    int pressTile_ = -1, focus_ = 0;
};

}  // namespace

std::unique_ptr<Scene> makeModeSelectScene() { return std::make_unique<ModesScene>(); }

}  // namespace liq
