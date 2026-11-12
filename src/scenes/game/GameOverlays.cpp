// GameScene full-screen overlays: tutorial, pause menu, game-over and puzzle result panels.
#include <ctime>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "game/ModeNames.hpp"
#include "scenes/game/GameScene.hpp"
#include "ui/widgets/Widgets.hpp"

namespace liq {

// Share card: the final pot with a caption, saved as a PNG next to the save file (the overlay is hidden for it).
// Run result as a picture (v1.8.4): the biggest drop you made as a real blob, plus icon chips for merges, drops and chain.
void GameScene::resultPicture(Ctx& c, float y) {
    Gfx& g = c.gfx;
    const Theme& t = *c.theme;
    const float cx = cfg::kW / 2.f;
    std::vector<BlobSprite> sp{plainSprite(cx - 62.f, y + 14.f, maxLevel_, 0.f, 0.f)};
    sp[0].ra = sp[0].rb = std::min(sp[0].ra, 15.f);
    c.raster.clear();
    c.raster.drawField(sp, t);
    c.raster.drawLabels(sp, t);
    g.raster(c.raster);
    // merges: two discs that overlap
    g.disc(cx - 14.f, y + 12.f, 5.f, t.ink);
    g.disc(cx - 14.f, y + 12.f, 4.f, t.blob[1]);
    g.disc(cx - 8.f, y + 12.f, 5.f, t.ink);
    g.disc(cx - 8.f, y + 12.f, 4.f, t.blob[2]);
    g.number(std::to_string(runMerges_), cx - 11.f, y + 24.f, 1, t.text, Align::Center);
    // drops: a falling droplet
    g.disc(cx + 22.f, y + 14.f, 5.f, t.ink);
    g.disc(cx + 22.f, y + 14.f, 4.f, t.blob[0]);
    g.rect(cx + 21.f, y + 5.f, 2, 4, t.ink);
    g.number(std::to_string(drops_), cx + 22.f, y + 24.f, 1, t.text, Align::Center);
    // best chain: crown
    g.icon(IconId::Crown, cx + 53.f, y + 7.f, t.accent);
    g.number("x" + std::to_string(runChain_), cx + 58.f, y + 24.f, 1, t.text, Align::Center);
}

void GameScene::captureCard(Ctx& c) {
    Gfx& g = c.gfx;
    const Theme& t = *c.theme;
    Loc& L = c.loc;
    c.ui.panel(35, 170, 200, 100);
    g.text(L.tr(mode_->nameKey()), cfg::kW / 2.f, 184, 1, t.accent, Align::Center);
    g.number(std::to_string(score_), cfg::kW / 2.f, 200, 2, t.text, Align::Center, TextFx::Outline, t.ink);
    resultPicture(c, 222.f);
    g.text("LIQ2048 V" LIQ_VERSION, cfg::kW / 2.f, 256, 1, t.dim, Align::Center);
    toast_ = L.tr(platform::imageToastKey(g.shareImage("liq2048-result-" + std::to_string(std::time(nullptr)) + ".png")));
    toastT_ = 2.5f;
}

void GameScene::tutorialDemo(Ctx& c, float cx, float cy) {
    const Theme& t = *c.theme;
    Gfx& g = c.gfx;
    const float tm = c.time;
    std::vector<BlobSprite> sp;
    if (tutPage_ == 0) {  // drag and drop
        const float x = cx + std::sin(tm * 1.6f) * 50.f;
        sp.push_back(plainSprite(x, cy - 12.f, 1, 0.05f, 1.57f));
        for (float y = cy + 2.f; y < cy + 26.f; y += 5.f) g.rect(std::floor(x), std::floor(y), 1, 2, t.dim);
        g.ring(x, cy + 34.f, 10, t.dim, 2);
    } else if (tutPage_ == 1) {  // merge
        const float p = std::fmod(tm, 2.4f) / 2.4f;
        if (p < 0.55f) {
            const float d = 24.f * (1.f - p / 0.55f) + 9.5f;
            sp.push_back(plainSprite(cx - d, cy, 1));
            sp.push_back(plainSprite(cx + d, cy, 1));
        } else {
            const float k = (p - 0.55f) / 0.45f;
            sp.push_back(plainSprite(cx, cy, 2, 0.15f * std::max(0.f, 1.f - k * 2.f), 0.f));
        }
    } else {  // keep below the line
        for (float x = cx - 60.f; x < cx + 60.f; x += 8.f) g.rect(x, cy - 12.f, 5, 1, t.danger);
        sp.push_back(plainSprite(cx - 18.f, cy + 10.f, 3));
        sp.push_back(plainSprite(cx + 14.f, cy + 14.f, 2));
        if (int(tm * 2.f) & 1) g.icon(IconId::Cross, cx - 3.f, cy - 30.f, t.danger);
    }
    c.raster.clear();
    c.raster.drawField(sp, t);
    c.raster.drawLabels(sp, t);
    g.raster(c.raster);
}

void GameScene::tutorial(Ctx& c) {
    Gfx& g = c.gfx;
    Ui& ui = c.ui;
    const Theme& t = *c.theme;
    Loc& L = c.loc;
    g.dim(t, 1);
    ui.panel(20, 118, 230, 262);
    g.text(L.tr("tut.title"), cfg::kW / 2.f, 140, 2, t.accent, Align::Center, TextFx::Outline, t.ink);
    // Demo box (drawn first so the droplets sit on top of its backing).
    g.rect(34, 160, 202, 82, t.tank);
    g.border(34, 160, 202, 82, 1, t.ink);
    tutorialDemo(c, cfg::kW / 2.f, 201.f);
    static const char* keys[3] = {"tut.1", "tut.2", "tut.3"};
    ui.paragraph(L.tr(keys[tutPage_]), 34, 262, 202, 1, t.text, Align::Left);
    widgets::pageDots(g, t, cfg::kW / 2.f, 336.f, 3, tutPage_);
    const bool last = tutPage_ == 2;
    if (ui.button(L.tr(last ? "tut.start" : "tut.next"), ui.mx(138, 92), 348, 92, 22, true, true, BtnStyle::Primary)) {
        if (last) endTutorial(c);
        else ++tutPage_;
    }
    if (ui.button(L.tr("tut.skip"), ui.mx(40, 92), 348, 92, 22) || c.in.esc) endTutorial(c);
}

void GameScene::endTutorial(Ctx& c) {
    c.save.tutorialDone = true;
    c.save.store();
    st_ = St::Playing;
    c.in.esc = false;
    coach_ = 0;
    setHint(c.loc.tr("coach.1"), -1.f);
}

void GameScene::pauseMenu(Ctx& c) {
    Gfx& g = c.gfx;
    Ui& ui = c.ui;
    const Theme& t = *c.theme;
    Loc& L = c.loc;
    g.dim(t, 1);
    const bool canCopy = !battle_ && !rec_.empty();
    const float ph = battle_ ? 138.f : (canCopy && !replaying_) ? 228.f : 198.f;  // bottom button ends 10 px above the panel edge
    ui.panel(55, 170, 160, ph);
    g.text(L.tr("pause.title"), cfg::kW / 2.f, 192, 2, t.accent, Align::Center, TextFx::Outline, t.ink);
    if (!platform::isMobile()) g.text("ESC  SPACE  U  F11", cfg::kW / 2.f, 170 + ph + 10, 1, t.dim, Align::Center);
    if (ui.button(L.tr("pause.resume"), 70, 214, 130, 24, true, true, BtnStyle::Primary) || c.in.esc) st_ = St::Playing;
    float by = 244;
    if (!battle_ && !replaying_) {
        if (ui.button(L.tr("pause.restart"), 70, by, 130, 24)) {
            commit(c);
            start(c);
            st_ = St::Playing;
        }
        by += 30;
    }
    if (canCopy) {  // the run so far, from the first drop to this moment
        if (ui.button(L.tr("pause.copycode"), 70, by, 130, 24)) {
            Replay part = rec_;
            if (!replaying_) {
                part.endStep = simStep_;
                part.outcome = 0;
            }
            SDL_SetClipboardText(part.toCode().c_str());
            toast_ = L.tr("over.copied");
            toastT_ = 2.f;
        }
        by += 30;
    }
    if (!battle_) {
        if (ui.button(L.tr("photo.btn"), 70, by, 130, 24)) st_ = St::Photo;
        by += 30;
    }
    if (ui.button(L.tr("pause.settings"), 70, by, 130, 24)) st_ = St::Settings;
    by += 30;
    if (ui.button(L.tr("pause.quit"), 70, by, 130, 24)) {
        if (battle_) {  // leaving a LAN match counts as a forfeit
            if (opp_->isLan() && !resultSet_) {
                ++c.save.battleLosses;
                resultSet_ = true;
            }
            c.save.store();
            c.go(SceneId::Battle);
        } else {
            commit(c);
            c.go(mid_ == ModeId::Puzzle ? SceneId::Puzzle : SceneId::Menu);
        }
    }
}

void GameScene::puzzleOverlay(Ctx& c, float oy) {
    Gfx& g = c.gfx;
    Ui& ui = c.ui;
    const Theme& t = *c.theme;
    Loc& L = c.loc;
    const PuzzleDef* pz = mode_->puzzle();
    const bool won = outcome_ == Outcome::Won;
    g.text(L.tr(won ? "over.solved" : "over.outofdrops"), cfg::kW / 2.f, 164 + oy, 2, won ? t.accent : t.danger, Align::Center,
           TextFx::Outline, t.ink);
    g.text(L.fmt("puzzle.level", std::to_string(c.puzzleLevel + 1)), cfg::kW / 2.f, 190 + oy, 1, t.dim, Align::Center);
    for (int i = 0; i < 3; ++i) {  // stars drawn as crowns, lit one after another
        const bool on = won && i < stars_ && overT_ > 0.25f + 0.25f * float(i);
        g.icon(IconId::Crown, cfg::kW / 2.f - 36.f + float(i) * 26.f, 206.f + oy, on ? t.accent : t.panelLo, 2);
    }
    g.text(L.tr("hud.drops") + " " + std::to_string(drops_) + "  " + L.fmt("puzzle.par", std::to_string(pz->par)), cfg::kW / 2.f,
           236.f + oy, 1, t.text, Align::Center);
    if (replaying_) {  // watching a code: only watch again or leave
        if (ui.button(L.tr("over.replay"), 55, 270 + oy, 160, 24, true, true, BtnStyle::Primary)) {
            start(c);
            st_ = St::Playing;
        }
        if (ui.button(L.tr("over.menu"), 55, 300 + oy, 160, 24) || c.in.esc) c.go(SceneId::Menu);
        return;
    }
    const bool hasNext = won && c.puzzleLevel + 1 < int(puzzlePack().size());
    float y = 258.f + oy;
    if (hasNext) {
        if (ui.button(L.tr("over.next"), 55, y, 160, 24, true, true, BtnStyle::Primary)) {
            ++c.puzzleLevel;
            start(c);
            st_ = St::Playing;
        }
        y += 30.f;
    }
    if (ui.button(L.tr(won ? "over.retry" : "over.restart"), 55, y, 160, 24, true, true, hasNext ? BtnStyle::Normal : BtnStyle::Primary)) {
        start(c);
        st_ = St::Playing;
    }
    y += 30.f;
    if (ui.button(L.tr("over.levels"), 55, y, 160, 24) || c.in.esc) c.go(SceneId::Puzzle);
    y += 30.f;
    if (!rec_.empty()) {  // share or rewatch the solution
        if (ui.button(L.tr("over.copycode"), 55, y, 78, 20)) {
            SDL_SetClipboardText(rec_.toCode().c_str());
            toast_ = L.tr("over.copied");
            toastT_ = 2.f;
        }
        if (ui.button(L.tr("over.replay"), 137, y, 78, 20)) {
            replaying_ = true;
            start(c);
            st_ = St::Playing;
        }
    }
}

void GameScene::overlay(Ctx& c) {
    if (mid_ == ModeId::Battle) {
        battleOverlay(c);
        return;
    }
    Gfx& g = c.gfx;
    Ui& ui = c.ui;
    const Theme& t = *c.theme;
    Loc& L = c.loc;
    g.dim(t, 0);
    const float oy = widgets::slideIn(overT_);
    ui.panel(35, 140 + oy, 200, mid_ == ModeId::Puzzle || hot_ ? 248 : 300);
    if (mid_ == ModeId::Puzzle) {
        puzzleOverlay(c, oy);
        return;
    }
    if (hot_) {
        const bool draw = hs_[0] == hs_[1];
        const int w = hs_[0] > hs_[1] ? 0 : 1;
        g.text(draw ? L.tr("hot.draw") : L.fmt("hot.wins", L.tr(w ? "hot.p2" : "hot.p1")), cfg::kW / 2.f, 170 + oy, 1, t.accent, Align::Center,
               TextFx::Outline, t.ink);
        for (int p = 0; p < 2; ++p) {
            const float y = 206.f + float(p) * 34.f + float(oy);
            g.text(L.tr(p ? "hot.p2" : "hot.p1"), 52, y, 1, t.dim, Align::Left);
            g.number(std::to_string(hs_[p]), 218, y, 2, !draw && w == p ? t.accent : t.text, Align::Right, TextFx::Outline, t.ink);
        }
        if (ui.button(L.tr("battle.rematch"), 55, 290 + oy, 160, 24, true, true, BtnStyle::Primary)) {
            start(c);
            st_ = St::Playing;
        }
        if (ui.button(L.tr("over.menu"), 55, 322 + oy, 160, 22) || c.in.esc) c.go(SceneId::Menu);
        return;
    }
    const char* key = outcome_ == Outcome::Won ? "over.won" : outcome_ == Outcome::Lost ? "over.lost" : "over.finished";
    g.text(L.tr(key), cfg::kW / 2.f, 164 + oy, 2, outcome_ == Outcome::Lost ? t.danger : t.accent, Align::Center,
           TextFx::Outline, t.ink);
    g.text(L.tr("over.score"), cfg::kW / 2.f, 192 + oy, 1, t.dim, Align::Center);
    const float k = clampv(overT_ / 0.8f, 0.f, 1.f);
    g.number(std::to_string(int(std::lround(float(score_) * (1.f - (1.f - k) * (1.f - k))))), cfg::kW / 2.f, 214 + oy, 3, t.text,
             Align::Center, TextFx::Outline, t.ink);
    g.text(L.tr("hud.best") + " " + std::to_string(bestFor(c)), cfg::kW / 2.f, 240 + oy, 1, t.dim, Align::Center);
    if (newBest_ && (int(overT_ * 3.f) % 2 == 0 || overT_ > 3.f)) {
        widgets::crownCaption(g, t, cfg::kW / 2.f, 257.f + oy, L.tr("over.newbest"));
    }
    resultPicture(c, 268.f + oy - (newBest_ ? 0.f : 10.f));
    const bool canReplay = !rec_.empty();
    if (canReplay) {
        if (ui.button(L.tr("over.copycode"), 55, 388 + oy, 78, 20)) {
            SDL_SetClipboardText(rec_.toCode().c_str());
            toast_ = L.tr("over.copied");
            toastT_ = 2.f;
        }
        if (ui.button(L.tr("over.copyres"), 137, 388 + oy, 78, 20)) {
            SDL_SetClipboardText(("Liq2048 V" LIQ_VERSION " - " + L.tr(modeNameKey(mid_)) + ": " + std::to_string(score_) + " pts, " +
                                  std::to_string(drops_) + " drops, chain x" + std::to_string(runChain_)).c_str());
            toast_ = L.tr("over.copied");
            toastT_ = 2.f;
        }
    }
    if (!replaying_ && ui.button(L.tr("card.shareimg"), 55, 412 + oy, 160, 20)) shotPending_ = true;
    if (ui.button(L.tr("over.restart"), 55, 308 + oy, 160, 24, true, true, BtnStyle::Primary)) {
        replaying_ = false;
        start(c);
        st_ = St::Playing;
    }
    if (canReplay && ui.button(L.tr("over.replay"), 55, 336 + oy, 160, 22)) {
        replaying_ = true;
        start(c);
        st_ = St::Playing;
    }
    if (ui.button(L.tr("over.menu"), 55, 362 + oy, 160, 22) || c.in.esc) c.go(SceneId::Menu);
}

}  // namespace liq
