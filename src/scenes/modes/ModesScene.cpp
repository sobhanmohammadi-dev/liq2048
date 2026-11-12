// Mode list: one screen, no scrolling. Tapping a tile opens the mode's information page (ModeInfoScene).
//
// Layout (logical pixels, 270 x 480), following the design sketch:
//
//   [Back]                Game mode
//   +--------------------------------+
//   | Battle                         |
//   +--------------------------------+
//   +--------+  +--------------------+
//   |        |  | Classic            |
//   | Puzzle |  +--------------------+
//   |        |  +--------------------+
//   |        |  | Zen                |
//   +--------+  +--------------------+
//   ----------- Limited challenges ---
//   +------------+  +----------------+
//   | Weekly     |  | Daily          |
//   +------------+  +----------------+
//   ----------------- Twists ---------
//   +-------+ +-------+ +-------+
//   | Timed | |Target | |  Big  |
//   +-------+ +-------+ +-------+
//   +-----------------+ +-----------+
//   | Madhouse        | | Blind     |
//   +-----------------+ +-----------+
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "core/Util.hpp"
#include "game/Mode.hpp"
#include "game/ModeNames.hpp"
#include "scenes/Scenes.hpp"
#include "scenes/modes/ModeIcons.hpp"
#include "ui/widgets/Widgets.hpp"

namespace liq {

namespace {

struct Tile {
    ModeId mode;
    float x, y, w, h;
    enum Shape { Wide, Tall, Small } shape;
};

const Tile kTiles[] = {
    {ModeId::Battle, 14, 48, 242, 72, Tile::Wide},
    {ModeId::Puzzle, 14, 126, 94, 118, Tile::Tall},
    {ModeId::Classic, 116, 126, 140, 56, Tile::Small},
    {ModeId::Zen, 116, 188, 140, 56, Tile::Small},
    {ModeId::Weekly, 14, 264, 119, 62, Tile::Small},
    {ModeId::Daily, 137, 264, 119, 62, Tile::Small},
    {ModeId::Timed, 14, 344, 78, 62, Tile::Small},
    {ModeId::Target, 96, 344, 78, 62, Tile::Small},
    {ModeId::BigBox, 178, 344, 78, 62, Tile::Small},
    {ModeId::Madhouse, 14, 412, 119, 62, Tile::Small},
    {ModeId::Blind, 137, 412, 119, 62, Tile::Small},
};
constexpr int kTileCount = int(sizeof(kTiles) / sizeof(kTiles[0]));
constexpr float kHeaderH = 44.f;

class ModesScene final : public Scene {
public:
    void frame(Ctx& c, float dt) override {
        age_ += dt;
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        Input& in = c.in;
        const bool rtl = L.rtl();

        // ---- input: tap a tile (press and release on the same tile) or move the focus with the keys -------------
        if (in.pressed) {
            pressTile_ = tileAt(c, in.mx, in.my);
            keyNav_ = false;
        }
        int open = -1;
        if (in.released && pressTile_ >= 0 && tileAt(c, in.mx, in.my) == pressTile_) open = pressTile_;
        if (in.released) pressTile_ = -1;
        if (in.dn || in.right) move(1);
        if (in.up || in.left) move(-1);
        if (in.confirm && keyNav_) open = focus_;

        // ---- draw ------------------------------------------------------------------------------------------------
        g.drawBackdrop(t, c.save.reduceMotion ? 0.f : c.time);
        section(c, 250.f, "select.limited");
        section(c, 330.f, "select.twists");

        std::vector<BlobSprite> sp;
        struct Label {
            int tile;
            float x, y;
        };
        std::vector<Label> labels;
        for (int i = 0; i < kTileCount; ++i) {
            const Tile& tl = kTiles[i];
            const float enter = modeIconSStep(0.f, 0.4f, age_ - float(i) * 0.04f);  // tiles settle in one after another
            const float x = ui.mx(tl.x, tl.w), y = tl.y + (1.f - enter) * 12.f;
            const bool held = pressTile_ == i && in.down && tileAt(c, in.mx, in.my) == i;
            const bool focus = keyNav_ && focus_ == i;
            const Color accent = t.blob[(int(tl.mode) * 5 + 3) % 12];
            Color fill = mix(t.panel, accent, 0.16f), hi = mix(t.panelHi, accent, 0.3f), lo = t.panelLo;
            if (held) {
                std::swap(hi, lo);
                fill = mix(fill, t.ink, 0.15f);
            }
            const float yy = y + (held ? 1.f : 0.f);
            g.panel(x, yy, tl.w, tl.h, fill, hi, lo, focus ? t.accent : t.ink);
            if (focus) g.border(x - 1, yy - 1, tl.w + 2, tl.h + 2, 1, t.accent);
            g.rect(rtl ? x + tl.w - 5 : x + 2, yy + 4, 3, tl.h - 8, accent);  // colour spine

            // The picture sits on the trailing side of wide tiles and above the name on the others.
            float icx = x + tl.w / 2.f, icy = yy + 24.f;
            if (tl.shape == Tile::Wide) icx = rtl ? x + 60.f : x + tl.w - 62.f, icy = yy + tl.h / 2.f;
            if (tl.shape == Tile::Tall) icy = yy + 52.f;
            if (tl.mode == ModeId::Classic || tl.mode == ModeId::Zen) icx = rtl ? x + 30.f : x + tl.w - 30.f, icy = yy + tl.h / 2.f;
            if (!c.save.reduceMotion) {  // a soft glow behind the picture that breathes in the mode's colour
                const float br = modeIconPulse(std::fmod(c.time / 3.f + float(i) * 0.13f, 1.f));
                g.disc(icx, icy, 16.f + 5.f * br, withAlpha(accent, 22 + int(18.f * br)));
            }
            drawModeIcon(c, tl.mode, icx, icy, sp);
            labels.push_back({i, x, yy});
        }
        if (!sp.empty()) {
            c.raster.clear();
            c.raster.drawField(sp, t);
            c.raster.drawLabels(sp, t);
            g.raster(c.raster);
        }
        for (const Label& lb : labels) {  // names and records above the droplets
            const Tile& tl = kTiles[lb.tile];
            const float x = lb.x, y = lb.y;
            const Align la = ui.start();
            const float tx = rtl ? x + tl.w - 12 : x + 12;
            const std::string name = L.tr(modeNameKey(tl.mode));
            if (tl.shape == Tile::Wide) {
                g.text(name, tx, y + 24, 2, t.text, la, TextFx::Outline, t.ink);
                record(c, tl.mode, tx, y + tl.h - 18, la);
            } else if (tl.shape == Tile::Tall) {
                g.text(name, x + tl.w / 2.f, y + 14, 1, t.text, Align::Center);
                record(c, tl.mode, x + tl.w / 2.f, y + tl.h - 14, Align::Center);
            } else if (tl.mode == ModeId::Classic || tl.mode == ModeId::Zen) {
                g.text(name, tx, y + 20, 1, t.text, la);
                record(c, tl.mode, tx, y + 38, la);
            } else {
                g.text(name, x + tl.w / 2.f, y + tl.h - 12, 1, t.text, Align::Center);
            }
        }

        // ---- fixed header ----------------------------------------------------------------------------------------
        g.rect(0, 0, float(cfg::kW), kHeaderH - 4.f, mix(t.sky[0], t.ink, 0.55f));
        g.rect(0, kHeaderH - 5.f, float(cfg::kW), 1, t.panelLo);
        g.text(L.tr("select.title"), float(cfg::kW) / 2.f, 20, 2, t.accent, Align::Center, TextFx::Outline, t.ink);
        if (ui.button(L.tr("common.back"), ui.mx(8.f, 52.f), 8, 52, 24) || in.esc) c.go(SceneId::Menu);

        if (!c.save.tutorialDone && c.save.coachStep == 1) {
            const Tile& ct = kTiles[2];
            widgets::coachRing(g, t, c.time, ui.mx(ct.x, ct.w), ct.y, ct.w, ct.h, L.tr("coach.modes"), true);
        }
        if (open >= 0) {
            if (c.save.coachStep == 1) c.save.coachStep = 2;
            c.audio.play(Sfx::Click);
            c.mode = kTiles[open].mode;
            c.go(SceneId::ModeInfo);
        }
    }

private:
    int tileAt(Ctx& c, float mx, float my) const {
        for (int i = 0; i < kTileCount; ++i) {
            const Tile& tl = kTiles[i];
            const float x = c.ui.mx(tl.x, tl.w);
            if (mx >= x && mx < x + tl.w && my >= tl.y && my < tl.y + tl.h) return i;
        }
        return -1;
    }
    void move(int d) {
        keyNav_ = true;
        focus_ = (focus_ + d + kTileCount) % kTileCount;
    }
    // A thin rule with a caption, e.g. "Limited challenges".
    void section(Ctx& c, float y, const char* key) const {
        const Theme& t = *c.theme;
        const std::string s = c.loc.tr(key);
        const float w = float(c.gfx.textW(s, 1)) + 14.f, cx = float(cfg::kW) / 2.f;
        c.gfx.rect(14, y + 5, cx - w / 2.f - 14.f, 1, t.panelLo);
        c.gfx.rect(cx + w / 2.f, y + 5, float(cfg::kW) - 14.f - (cx + w / 2.f), 1, t.panelLo);
        c.gfx.text(s, cx, y + 6, 1, t.dim, Align::Center);
    }
    // Personal record shown on the big tiles.
    void record(Ctx& c, ModeId m, float x, float y, Align a) const {
        const Theme& t = *c.theme;
        std::string s;
        if (m == ModeId::Battle) s = std::to_string(c.save.battleWins) + " - " + std::to_string(c.save.battleLosses) + " - " + std::to_string(c.save.battleDraws);
        else if (m == ModeId::Puzzle) s = std::to_string(c.save.puzzlesSolved());
        else {
            const int best = c.save.best[int(m)];
            if (best <= 0) return;
            s = std::to_string(best);
        }
        c.gfx.number(s, x, y, 1, t.accent, a);
    }

    float age_ = 0.f;
    int pressTile_ = -1, focus_ = 0;
    bool keyNav_ = false;
};

}  // namespace

std::unique_ptr<Scene> makeModeSelectScene() { return std::make_unique<ModesScene>(); }

}  // namespace liq
