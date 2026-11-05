// Menu, mode selection, progress and standalone settings scenes.
#include <cmath>
#include <string>

#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "core/Version.hpp"
#include "game/Mode.hpp"
#include "game/World.hpp"
#include "scenes/Scenes.hpp"

namespace liq {

namespace {

// A row of wobbling droplets that touch and fuse, as a living logo.
void drawLogoCluster(Ctx& c, float cy) {
    std::vector<BlobSprite> sp;
    float total = 0.f;
    for (int i = 0; i < 7; ++i) total += World::radiusFor(i) * 2.f - 5.f;
    float x = cfg::kW / 2.f - total / 2.f;
    for (int i = 0; i < 7; ++i) {
        const float r = World::radiusFor(i);
        const float t = c.time;
        const float px = x + r + std::sin(t * 1.1f + float(i) * 1.3f) * 2.5f;
        const float py = cy + 12.f - r * 0.55f + std::sin(t * 1.5f + float(i) * 0.9f) * 4.f;
        sp.push_back(plainSprite(px, py, i, 0.07f + 0.04f * std::sin(t * 1.9f + float(i)), t * 0.7f + float(i)));
        x += r * 2.f - 5.f;
    }
    c.raster.clear();
    c.raster.drawField(sp, *c.theme);
    c.raster.drawLabels(sp, *c.theme);
    c.gfx.raster(c.raster);
}

class MenuScene final : public Scene {
public:
    void frame(Ctx& c, float dt) override {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        g.drawBackdrop(t, c.save.reduceMotion ? 0.f : c.time);
        drawWordmark(c, cfg::kW / 2.f, 70.f + std::sin(c.time * 1.6f) * 1.f, 5);
        g.text(L.tr("menu.tagline"), cfg::kW / 2.f, 106.f, 1, t.dim, Align::Center);
        drawLogoCluster(c, 160.f);

        const float bw = 150, bx = (cfg::kW - bw) / 2.f;
        float y = 222;
        if (ui.button(L.tr("menu.start"), bx, y, bw, 28, true, true, BtnStyle::Primary)) c.go(SceneId::ModeSelect);
        y += 35;
        if (ui.button(L.tr("menu.extras"), bx, y, bw, 28)) c.go(SceneId::Extras);
        y += 35;
        if (ui.button(L.tr("menu.progress"), bx, y, bw, 28)) c.go(SceneId::Progress);
        y += 35;
        if (ui.button(L.tr("menu.settings"), bx, y, bw, 28)) c.go(SceneId::Settings);
        if (badCodeT_ > 0.f) {
            badCodeT_ -= dt;
            g.text(L.tr("menu.badcode"), cfg::kW / 2.f, 416.f, 1, t.danger, Align::Center, TextFx::Outline, t.ink);
        }

        g.icon(IconId::Crown, cfg::kW / 2.f - float(g.textW(L.fmt("menu.streak", std::to_string(c.save.streak)), 1)) / 2.f - 14.f,
               432.f, t.accent);
        g.text(L.fmt("menu.streak", std::to_string(c.save.streak)), cfg::kW / 2.f, 435.f, 1, t.dim, Align::Center);
        g.number("V" LIQ_VERSION, cfg::kW - 6.f, 470.f, 1, mix(t.dim, t.sky[3], 0.4f), Align::Right);
        if (ui.button(L.tr("menu.authors"), 8, 454, 64, 16)) c.go(SceneId::Credits);
        if (c.in.esc && !platform::isMobile()) c.quit = true;
    }

private:
    float badCodeT_ = 0.f;
};

class PuzzleSelectScene final : public Scene {
public:
    void frame(Ctx& c, float) override {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        g.drawBackdrop(t, c.save.reduceMotion ? 0.f : c.time);
        g.text(L.tr("mode.puzzle"), cfg::kW / 2.f, 28, 2, t.accent, Align::Center, TextFx::Outline, t.ink);
        const auto& pack = puzzlePack();
        int stars = 0;
        for (size_t i = 0; i < pack.size() && i < size_t(Save::kPuzzleMax); ++i) stars += c.save.puzzleStars[i];
        g.text(std::to_string(stars) + " / " + std::to_string(pack.size() * 3) + "  " + L.tr("puzzle.stars"), cfg::kW / 2.f, 50, 1, t.dim, Align::Center);
        const int cols = 5, per = 30;
        const int pages = std::max(1, (int(pack.size()) + per - 1) / per);
        const float bw = 40.f, gap = 6.f, x0 = (cfg::kW - (cols * bw + (cols - 1) * gap)) / 2.f;
        for (int k = 0; k < per; ++k) {
            const int i = page_ * per + k;
            if (i >= int(pack.size())) break;
            const float x = x0 + float(k % cols) * (bw + gap), y = 68.f + float(k / cols) * 52.f;
            const int st = c.save.puzzleStars[i];
            if (ui.button(std::to_string(i + 1), x, y, bw, 44, true, true, st == 0 ? BtnStyle::Normal : BtnStyle::Normal)) {
                c.mode = ModeId::Puzzle;
                c.puzzleLevel = i;
                c.go(SceneId::Game);
            }
            for (int s = 0; s < 3; ++s)  // three tiny stars under the number
                g.rect(x + bw / 2.f - 11.f + float(s) * 8.f, y + 31.f, 5, 5, s < st ? t.accent : t.panelLo);
        }
        if (pages > 1 && ui.button(">", 148, 408, 100, 26)) page_ = (page_ + 1) % pages;
        if (ui.button( L.tr("common.back"), 22.f, 408.f, static_cast<float>(pages > 1 ? 100 : 226), 26.f) || c.in.esc) {c.go(SceneId::ModeSelect);}
    }

private:
    int page_ = 0;
};

class CreditsScene final : public Scene {
public:
    void frame(Ctx& c, float) override {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        g.drawBackdrop(t, c.save.reduceMotion ? 0.f : c.time);
        g.text(L.tr("credits.title"), cfg::kW / 2.f, 40, 2, t.accent, Align::Center, TextFx::Outline, t.ink);
        ui.panel(14, 64, 242, 330);
        const float cx = cfg::kW / 2.f;
        g.text(L.tr("credits.made"), cx, 92, 1, t.dim, Align::Center);
        g.text("SOBHAN MOHAMMADI", cx, 118, 2, t.text, Align::Center, TextFx::Outline, t.ink);
        g.rect(40, 150, 190, 1, t.panelLo);
        g.text(L.tr("credits.thanks"), cx, 172, 1, t.accent, Align::Center);
        struct Row { const char* name; const char* key; };
        static const Row rows[3] = {{"SDL2", "credits.sdl"}, {"VAZIRMATN", "credits.font"}, {"CLAUDE", "credits.audio"}};
        float y = 204;
        for (const Row& r : rows) {
            g.text(r.name, cx, y, 1, t.text, Align::Center, TextFx::Outline, t.ink);
            g.text(L.tr(r.key), cx, y + 16, 1, t.dim, Align::Center);
            y += 54;
        }
        if (ui.button(L.tr("common.back"), 22, 408, 226, 26) || c.in.esc) c.go(SceneId::Menu);
    }
};

class ProgressScene final : public Scene {
public:
    void frame(Ctx& c, float) override {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        g.drawBackdrop(t, c.save.reduceMotion ? 0.f : c.time);
        g.text(L.tr("progress.title"), cfg::kW / 2.f, 28, 2, t.accent, Align::Center, TextFx::Outline, t.ink);
        ui.panel(14, 44, 242, 360);

        // Tabs: scores, awards, statistics.
        static const char* tabs[3] = {"tab.scores", "tab.awards", "tab.stats"};
        for (int i = 0; i < 3; ++i) {
            const float bx = ui.mx(20.f + float(i) * 78.f, 74.f);
            if (ui.button(L.tr(tabs[i]), bx, 52, 74, 20, true, true, tab_ == i ? BtnStyle::Primary : BtnStyle::Normal)) tab_ = i;
        }
        const bool rtl = L.rtl();
        const float lx = rtl ? 244.f : 26.f, rx = rtl ? 26.f : 244.f;
        const Align la = ui.start(), ra = rtl ? Align::Left : Align::Right;
        g.rect(22, 80, 226, 1, t.panelLo);
        if (tab_ == 0) {
            static const char* names[kModeCount] = {"mode.classic", "mode.timed", "mode.target", "mode.daily", "mode.zen",
                                                    "mode.rain",    "mode.tide",  "mode.weekly", "mode.puzzle", "mode.battle"};
            // Medal thresholds (bronze, silver, gold) per mode.
            static const int medal[kModeCount][3] = {{2000, 6000, 15000}, {800, 2000, 4000}, {500, 2000, 6000}, {1500, 4000, 9000}, {2000, 6000, 15000},
                                                     {600, 1800, 4500},   {800, 2500, 6000}, {1500, 4000, 9000}, {0, 0, 0}, {0, 0, 0}};
            for (int i = 0; i < kModeCount; ++i) {
                const float y = 96.f + float(i) * 30.f;
                g.text(L.tr(names[i]), lx, y, 1, t.text, la);
                const int v = i == int(ModeId::Puzzle) ? c.save.puzzlesSolved() : i == int(ModeId::Battle) ? c.save.battleWins
                              : i == int(ModeId::Weekly) ? c.save.weeklyBest : c.save.best[i];
                g.number(std::to_string(v), rx, y, 1, t.text, ra);
                if (i != int(ModeId::Puzzle) && i != int(ModeId::Battle)) {
                    int m = 0;
                    for (int k = 0; k < 3; ++k) m += v >= medal[i][k];
                    static const uint32_t col[3] = {0xcd7f32, 0xc0c8d8, 0xffd23d};
                    for (int k = 0; k < 3; ++k) {
                        const float mx = rtl ? 244.f - float(k) * 10.f - 8.f : 26.f + float(k) * 10.f;
                        g.disc(mx + 4.f, y + 14.f, 3, k < m ? t.ink : t.panelLo);
                        g.disc(mx + 4.f, y + 14.f, 2, k < m ? rgb(col[std::min(k, 2)]) : t.panelLo);
                    }
                }
            }
        } else if (tab_ == 1) {
            for (int i = 0; i < kAchCount; ++i) {
                const float y = 94.f + float(i) * 15.f;
                const bool got = c.save.has(i);
                const float ix = rtl ? 244.f - 7.f : 26.f;
                g.icon(got ? IconId::Check : IconId::Lock, ix, y - 3.f, got ? t.accent : t.dim);
                g.text(L.tr(achKey(i)), rtl ? 230.f : 42.f, y, 1, got ? t.text : t.dim, la);
            }
        } else {
            const Save& sv = c.save;
            const int h = sv.playSeconds / 3600, m = sv.playSeconds / 60 % 60;
            const std::string tm = std::to_string(h) + "H " + std::to_string(m) + "M";
            struct Row { const char* key; std::string val; };
            const Row rows[] = {{"stats.games", std::to_string(sv.gamesPlayed)}, {"stats.drops", std::to_string(sv.totalDrops)},
                                {"stats.merges", std::to_string(sv.totalMerges)}, {"stats.chain", std::to_string(sv.longestChain)},
                                {"stats.tile", std::to_string(1 << sv.highestLevel)}, {"stats.time", tm},
                                {"stats.puzzles", std::to_string(sv.puzzlesSolved())}, {"stats.streak", std::to_string(sv.streak)}};
            for (int i = 0; i < 8; ++i) {
                const float y = 100.f + float(i) * 26.f;
                g.text(L.tr(rows[i].key), lx, y, 1, t.text, la);
                g.number(rows[i].val, rx, y, 1, t.accent, ra);
            }
        }
        if (ui.button(L.tr("common.back"), 60, 414, 150, 26) || c.in.esc) c.go(SceneId::Menu);
    }

private:
    int tab_ = 0;
};

class SettingsScene final : public Scene {
public:
    void frame(Ctx& c, float) override {
        c.gfx.drawBackdrop(*c.theme, c.save.reduceMotion ? 0.f : c.time);
        if (drawSettings(c, 72) || c.in.esc) c.go(SceneId::Menu);
    }
};

}  // namespace

std::unique_ptr<Scene> makeMenuScene() { return std::make_unique<MenuScene>(); }
std::unique_ptr<Scene> makeCreditsScene() { return std::make_unique<CreditsScene>(); }
std::unique_ptr<Scene> makeProgressScene() { return std::make_unique<ProgressScene>(); }
std::unique_ptr<Scene> makeSettingsScene() { return std::make_unique<SettingsScene>(); }
std::unique_ptr<Scene> makePuzzleSelectScene() { return std::make_unique<PuzzleSelectScene>(); }

std::unique_ptr<Scene> makeScene(SceneId id, Ctx& c) {
    switch (id) {
        case SceneId::ModeSelect: return makeModeSelectScene();
        case SceneId::Progress: return makeProgressScene();
        case SceneId::Settings: return makeSettingsScene();
        case SceneId::Puzzle: return makePuzzleSelectScene();
        case SceneId::Credits: return makeCreditsScene();
        case SceneId::Battle: return makeBattleScene();
        case SceneId::Extras: return makeExtrasScene();
        case SceneId::Card: return makeCardScene();
        case SceneId::Game: return makeGameScene(c);
        case SceneId::Brand: return makeBrandScene();
        default: return makeMenuScene();
    }
}

}  // namespace liq
