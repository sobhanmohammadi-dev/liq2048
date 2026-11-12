// Menu, mode selection, progress and standalone settings scenes.
#include <cmath>
#include <cstdlib>
#include <string>

#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "core/Version.hpp"
#include "game/Mode.hpp"
#include "game/ModeNames.hpp"
#include "game/World.hpp"
#include "scenes/Scenes.hpp"
#include "ui/widgets/Widgets.hpp"

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
        if (!resumeChecked_) {  // is there an unfinished run from this build that can be continued?
            resumeChecked_ = true;
            resumeOk_ = !c.save.resumeCode.empty() && Replay::fromCode(c.save.resumeCode, resumeRep_) && resumeRep_.physics == cfg::kPhysicsVersion &&
                        resumeRep_.mode >= 0 && resumeRep_.mode < kModeCount && !modeRetired(ModeId(resumeRep_.mode));
            if (!resumeOk_) c.save.resumeCode.clear();
        }
        if (resumeOk_) {
            if (ui.button(L.tr("menu.continue"), bx, y, bw, 28, true, true, BtnStyle::Primary)) {
                c.pendingReplay = resumeRep_;
                c.hasPendingReplay = true;
                c.resumeRun = true;
                c.go(SceneId::Game);
            }
            g.text(L.tr(modeNameKey(ModeId(resumeRep_.mode))), cfg::kW / 2.f, y + 31.f, 1, t.dim, Align::Center);
            y += 44;
        }
        if (ui.button(L.tr("menu.start"), bx, y, bw, 28, true, !resumeOk_, resumeOk_ ? BtnStyle::Normal : BtnStyle::Primary)) {
            if (c.save.coachStep == 0) c.save.coachStep = 1;
            c.go(SceneId::ModeSelect);
        }
        if (!c.save.tutorialDone && c.save.coachStep == 0 && !resumeOk_)
            widgets::coachRing(g, t, c.time, bx, y, bw, 28, L.tr("coach.menu"), false);
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
    bool resumeChecked_ = false, resumeOk_ = false;
    Replay resumeRep_;
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
        if (ui.button( L.tr("common.back"), 22.f, 408.f, static_cast<float>(pages > 1 ? 100 : 226), 26.f) || c.in.esc) {c.go(SceneId::ModeInfo);}
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
        static const char* tabs[4] = {"tab.profile", "tab.scores", "tab.awards", "tab.history"};
        for (int i = 0; i < 4; ++i) {
            const float bx = ui.mx(14.f + float(i) * 59.f, 56.f);
            if (ui.button(L.tr(tabs[i]), bx, 52, 56, 20, true, true, tab_ == i ? BtnStyle::Primary : BtnStyle::Normal)) tab_ = i;
        }
        const bool rtl = L.rtl();
        const float lx = rtl ? 244.f : 26.f, rx = rtl ? 26.f : 244.f;
        const Align la = ui.start(), ra = rtl ? Align::Left : Align::Right;
        g.rect(22, 80, 226, 1, t.panelLo);
        if (tab_ == 1) {
            // Medal thresholds (bronze, silver, gold) per mode.
            static const int medal[kModeCount][3] = {{2000, 6000, 15000}, {800, 2000, 4000}, {500, 2000, 6000}, {1500, 4000, 9000}, {2000, 6000, 15000},
                                                     {600, 1800, 4500},   {800, 2500, 6000}, {1500, 4000, 9000}, {0, 0, 0}, {0, 0, 0},
                                                     {1500, 4000, 9000}, {1500, 4000, 9000}, {1500, 4000, 9000}, {1000, 3000, 7000}};
            int row = 0;
            for (int i = 0; i < kModeCount; ++i) {
                if (modeRetired(ModeId(i))) continue;
                if ((i == int(ModeId::Rain) || i == int(ModeId::Tide)) && c.save.best[i] <= 0) continue;  // now modifiers
                const float y = 96.f + float(row++) * 30.f;
                g.text(L.tr(modeNameKey(ModeId(i))), lx, y, 1, t.text, la);
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
        } else if (tab_ == 2) {
            // Awards as a grid of medals: lit and ticked once earned, padlocked before. Tap one to read its name.
            if (c.in.pressed)
                for (int i = 0; i < kAchCount; ++i) {
                    const float cx = 22.f + 57.f * float(i % 4), cy = 88.f + 52.f * float(i / 4);
                    if (c.in.mx >= cx && c.in.mx < cx + 55.f && c.in.my >= cy && c.in.my < cy + 50.f) pick_ = i;
                }
            int got = 0;
            for (int i = 0; i < kAchCount; ++i) got += c.save.has(i);
            for (int i = 0; i < kAchCount; ++i) {
                const float cx = 22.f + 57.f * float(i % 4) + 27.f, cy = 88.f + 52.f * float(i / 4) + 22.f;
                const bool has = c.save.has(i);
                const Color base = has ? rgb(i < 5 ? 0xffd23d : i < 9 ? 0x6fd3ff : 0xff8fd0) : t.panelLo;
                if (pick_ == i) g.border(cx - 26.f, cy - 21.f, 52.f, 46.f, 1, t.accent);
                g.disc(cx, cy, 15.f, t.ink);
                g.disc(cx, cy, 13.f, base);
                g.disc(cx, cy - 2.f, 8.f, has ? mix(base, rgb(0xffffff), 0.35f) : mix(t.panelLo, t.ink, 0.3f));
                g.icon(has ? IconId::Check : IconId::Lock, cx - 3.f, cy - 4.f, has ? t.ink : t.dim);
                if (i < 5) g.number(std::to_string(128 << i), cx, cy + 20.f, 1, has ? t.accent : t.dim, Align::Center);
            }
            g.number(std::to_string(got) + "/" + std::to_string(kAchCount), cfg::kW / 2.f, 354.f, 1, t.accent, Align::Center);
            if (pick_ >= 0) ui.paragraph(L.tr(achKey(pick_)), 20, 372, 230, 1, c.save.has(pick_) ? t.text : t.dim, Align::Center);
        } else if (tab_ == 3) {
            static const ModeId hm[6] = {ModeId::Classic, ModeId::Timed, ModeId::Rain, ModeId::Tide, ModeId::Zen, ModeId::Target};
            static const char* hn[6] = {"mode.classic", "mode.timed", "mode.rain", "mode.tide", "mode.zen", "mode.target"};
            for (int r = 0; r < 6; ++r) {
                const float y0 = 88.f + float(r) * 54.f;
                const std::vector<int>& h = c.save.hist[int(hm[r])];
                g.text(L.tr(hn[r]), lx, y0 + 4, 1, t.text, la);
                int mx = 1;
                for (int v : h) mx = std::max(mx, v);
                if (!h.empty()) g.number(std::to_string(mx), rx, y0 + 4, 1, t.accent, ra);
                const float base = y0 + 46.f;
                g.rect(22, base, 226, 1, t.panelLo);
                for (size_t k = 0; k < h.size(); ++k) {
                    const float bh = std::max(1.f, 30.f * float(h[k]) / float(mx));
                    const float bx = rtl ? 248.f - 17.f - float(k) * 18.f : 24.f + float(k) * 18.f;
                    const bool last = k + 1 == h.size();
                    g.rect(bx, base - bh, 14, bh, last ? t.accent : mix(t.dim, t.sky[3], 0.4f));
                }
                if (h.empty()) g.text("-", 26, base - 12, 1, t.dim, Align::Left);
            }
        } else {
            drawProfile(c);
        }
        if (ui.button(L.tr("common.back"), 60, 414, 150, 26) || c.in.esc) c.go(SceneId::Menu);
    }

private:
    int tab_ = std::getenv("LIQ_TAB") ? std::atoi(std::getenv("LIQ_TAB")) & 3 : 0, pick_ = std::getenv("LIQ_TAB") ? 4 : -1;  // dev: screenshots

    // Profile as a picture: your biggest drop as a real blob, the player's name and streak, then icon chips for the numbers.
    void drawProfile(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        const Save& sv = c.save;
        const bool rtl = L.rtl();
        // hero: blob + name
        const float bx = rtl ? 196.f : 74.f, tx = rtl ? 126.f : 150.f;
        std::vector<BlobSprite> sp{plainSprite(bx, 130.f, sv.highestLevel, 0.05f, c.time)};
        sp[0].ra = sp[0].rb = std::min(sp[0].ra * 1.5f, 38.f);
        c.raster.clear();
        c.raster.drawField(sp, t);
        c.raster.drawLabels(sp, t);
        g.raster(c.raster);
        const std::string name = sv.playerName.empty() ? L.tr("tab.profile") : sv.playerName;
        g.text(name, tx, 104, 1, t.text, Align::Center, TextFx::Outline, t.ink);
        g.icon(IconId::Crown, tx - 22.f, 122.f, t.accent);
        g.number(std::to_string(sv.streak), tx + 6.f, 123.f, 1, t.accent, Align::Left);
        int medals = 0;
        for (int i = 0; i < kAchCount; ++i) medals += sv.has(i);
        g.disc(tx - 14.f, 148.f, 6.f, t.ink);
        g.disc(tx - 14.f, 148.f, 5.f, rgb(0xffd23d));
        g.number(std::to_string(medals), tx + 6.f, 145.f, 1, t.text, Align::Left);
        g.rect(22, 184, 226, 1, t.panelLo);
        // chips
        const int h = sv.playSeconds / 3600, m = sv.playSeconds / 60 % 60;
        const std::string tm = std::to_string(h) + "H " + std::to_string(m) + "M";
        struct Chip { int glyph; std::string val; };
        const Chip chips[] = {{0, std::to_string(sv.gamesPlayed)}, {1, std::to_string(sv.totalDrops)}, {2, std::to_string(sv.totalMerges)},
                              {3, std::to_string(sv.longestChain)}, {4, std::to_string(1 << sv.highestLevel)}, {5, tm},
                              {6, std::to_string(sv.puzzlesSolved())}, {7, std::to_string(sv.battleWins)}};
        for (int i = 0; i < 8; ++i) {
            const float cx = 22.f + 115.f * float(i % 2), cy = 194.f + 50.f * float(i / 2);
            const float x = rtl ? 248.f - (cx - 22.f) - 111.f : cx;
            g.rect(x, cy, 111, 44, mix(t.panel, t.blob[(i * 3) % 12], 0.14f));
            g.border(x, cy, 111, 44, 1, t.ink);
            const float gx = rtl ? x + 111.f - 20.f : x + 20.f, gy = cy + 22.f;
            glyph(c, chips[i].glyph, gx, gy);
            g.number(chips[i].val, rtl ? x + 111.f - 38.f : x + 38.f, cy + 19.f, 1, t.text, rtl ? Align::Right : Align::Left);
        }
    }

    // Small hand-drawn pictures for the profile chips.
    static void glyph(Ctx& c, int id, float x, float y) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        auto bead = [&](float bx, float by, float r, Color col) {
            g.disc(bx, by, r + 1.f, t.ink);
            g.disc(bx, by, r, col);
        };
        switch (id) {
            case 0: g.icon(IconId::Play, x - 4.f, y - 5.f, t.accent); break;                                 // games
            case 1: bead(x, y + 2.f, 6.f, t.blob[0]); g.rect(x - 1.f, y - 9.f, 3, 5, t.ink); break;        // drops
            case 2: bead(x - 5.f, y, 6.f, t.blob[1]); bead(x + 5.f, y, 6.f, t.blob[3]); break;             // merges
            case 3: g.icon(IconId::Crown, x - 6.f, y - 5.f, t.accent); break;                              // best chain
            case 4: bead(x, y, 9.f, t.blob[6]); break;                                                      // highest tile
            case 5:                                                                                         // play time
                bead(x, y, 9.f, t.panelHi);
                g.rect(x, y - 6.f, 1, 6, t.ink);
                g.rect(x, y, 5, 1, t.ink);
                break;
            case 6: g.icon(IconId::Check, x - 4.f, y - 4.f, t.accent); break;                              // puzzles
            default: bead(x - 4.f, y + 3.f, 5.f, t.blob[2]); bead(x + 5.f, y - 3.f, 5.f, t.danger); break;  // battles won
        }
    }
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
        case SceneId::ModeInfo: return makeModeInfoScene();
        case SceneId::Progress: return makeProgressScene();
        case SceneId::Settings: return makeSettingsScene();
        case SceneId::Puzzle: return makePuzzleSelectScene();
        case SceneId::Credits: return makeCreditsScene();
        case SceneId::Battle: return makeBattleScene();
        case SceneId::Party: return makePartyScene();
        case SceneId::Tilt: return makeTiltScene();
        case SceneId::Extras: return makeExtrasScene();
        case SceneId::Card: return makeCardScene();
        case SceneId::ThemeEditor: return makeThemeEditorScene();
        case SceneId::Game: return makeGameScene(c);
        case SceneId::Brand: return makeBrandScene();
        default: return makeMenuScene();
    }
}

}  // namespace liq
