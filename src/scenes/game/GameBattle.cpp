// GameScene Battle support: opponent link, countdown, result and the Battle end-of-match overlay.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "scenes/game/GameScene.hpp"
#include "ui/widgets/Widgets.hpp"

namespace liq {

battle::SelfInfo GameScene::selfInfo() const {
    battle::SelfInfo me;
    me.score = score_;
    me.maxLevel = maxLevel_;
    me.drops = drops_;
    me.finished = myDone_;
    me.toppedOut = toppedOut_;
    me.elapsed = elapsed_;
    me.world = &world_;
    me.aimX = aimX_;
    me.heldLevel = cur_;
    return me;
}

void GameScene::battleFrame(Ctx& c, float dt, float slow) {
    const bool lan = opp_->isLan();
    const bool menuUp = st_ == St::Paused || st_ == St::Settings;
    if (countdown_ > 0.f) {  // 3, 2, 1, GO
        if (!menuUp || lan) countdown_ -= dt;
        const int n = int(std::ceil(countdown_));
        if (n != lastTick_ && n >= 0) {
            lastTick_ = n;
            c.audio.play(n > 0 ? Sfx::Click : Sfx::Swipe);
        }
        if (lan) opp_->update(dt, selfInfo());  // keep the link alive
        return;
    }
    if (!menuUp || lan) {  // a bot match really pauses; a LAN match cannot
        opp_->fastForward(myDone_);
        opp_->update(dt, selfInfo());
    }
    if (st_ == St::Playing) simulate(c, std::min(dt, 0.05f) * slow);
    else if (menuUp && lan) simulate(c, std::min(dt, 0.05f) * slow, true);
    if (!resultSet_) {
        if (opp_->disconnected()) {
            oppLeft_ = true;
            if (!myDone_) {
                myDone_ = true;
                outcome_ = Outcome::Finished;
                st_ = St::Over;
                overT_ = 0;
                commit(c);
            }
            setResult(c);
        } else if (myDone_ && opp_->finished()) {
            setResult(c);
        }
    }
}

void GameScene::setResult(Ctx& c) {
    resultSet_ = true;
    const int mine = score_, theirs = opp_->score();
    if (oppLeft_ || mine > theirs) result_ = Result::Win;
    else if (mine < theirs) result_ = Result::Loss;
    else if (maxLevel_ != opp_->maxLevel()) result_ = maxLevel_ > opp_->maxLevel() ? Result::Win : Result::Loss;
    else result_ = Result::Draw;
    if (result_ == Result::Win) ++c.save.battleWins;
    else if (result_ == Result::Loss) ++c.save.battleLosses;
    else ++c.save.battleDraws;
    c.save.store();
    c.audio.play(result_ == Result::Loss ? Sfx::Fail : Sfx::Unlock);
    overT_ = 0;
}

void GameScene::rematch(Ctx& c) {
    bcfg_.seed = bcfg_.seed * 1664525u + 1013904223u;
    opp_ = battle::makeBot(bcfg_);
    start(c);
    st_ = St::Playing;
}

void GameScene::drawViewButton(Ctx& c) {
    if (resultSet_ && viewOpp_ == false && st_ == St::Over) {
        // still offered after the result, so the opponent's final pot can be inspected
    }
    const bool can = st_ == St::Playing || st_ == St::Over || countdown_ > 0.f;
    const std::string label = c.loc.tr(viewOpp_ ? "battle.back" : "battle.view");
    if (c.ui.button(label, 22, 450, 226, 22, can, can, viewOpp_ ? BtnStyle::Primary : BtnStyle::Normal)) viewOpp_ = !viewOpp_;
}

void GameScene::drawCountdown(Ctx& c) {
    Gfx& g = c.gfx;
    const Theme& t = *c.theme;
    g.dim(t, 0);
    const float frac = countdown_ - std::floor(countdown_);
    const int n = int(std::ceil(countdown_));
    const int scale = 8 + (c.save.reduceMotion ? 0 : int(frac * 4.f));
    if (n >= 1 && n <= 3) g.number(std::to_string(n), cfg::kW / 2.f, 230, scale, t.accent, Align::Center, TextFx::Outline, t.ink);
    else g.text(c.loc.tr("battle.go"), cfg::kW / 2.f, 230, 5, t.accent, Align::Center, TextFx::Outline, t.ink);
    g.text(opp_->name(), cfg::kW / 2.f, 290, 1, t.text, Align::Center, TextFx::Outline, t.ink);
    g.text("VS", cfg::kW / 2.f, 276, 1, t.dim, Align::Center);
}

void GameScene::battleOverlay(Ctx& c) {
    if (viewing()) return;  // the opponent's pot is on screen; the button below leads back
    Gfx& g = c.gfx;
    Ui& ui = c.ui;
    const Theme& t = *c.theme;
    Loc& L = c.loc;
    g.dim(t, 0);
    const float oy = widgets::slideIn(overT_);
    const float px = 25, pw = 220, py = 96 + oy;
    ui.panel(px, py, pw, 330);
    const float cx = cfg::kW / 2.f;
    if (!resultSet_) {
        g.text(L.tr("battle.waitopp"), cx, py + 26, 1, t.accent, Align::Center);
        widgets::spinner(g, t, cx, py + 44.f, c.time);
    } else {
        const bool win = result_ == Result::Win, loss = result_ == Result::Loss;
        g.text(L.tr(win ? "battle.victory" : loss ? "battle.defeat" : "battle.draw"), cx, py + 28, 2, win ? t.accent : loss ? t.danger : t.text,
               Align::Center, TextFx::Outline, t.ink);
        if (oppLeft_) g.text(L.tr("battle.oppleft"), cx, py + 50, 1, t.dim, Align::Center);
        else if (toppedOut_) g.text(L.tr("battle.toppedout"), cx, py + 50, 1, t.dim, Align::Center);
        else if (opp_->toppedOut()) g.text(L.tr("battle.oppout"), cx, py + 50, 1, t.dim, Align::Center);
    }
    // Two columns: you and the opponent, with the biggest droplet each reached.
    const float lx = px + 56, rx = px + pw - 56;
    g.rect(cx, py + 74, 1, 96, t.panelLo);
    g.text("VS", cx, py + 122, 1, t.dim, Align::Center);
    auto column = [&](float x, const std::string& name, int score, int level, bool winner) {
        g.text(name, x, py + 80, 1, t.dim, Align::Center);
        g.number(std::to_string(score), x, py + 108, 2, winner ? t.accent : t.text, Align::Center, TextFx::Outline, t.ink);
        widgets::chip(g, t, x, py + 142, 10, t.blob[std::min(std::max(level, 0), 11)]);
        g.number(std::to_string(1 << std::min(std::max(level, 0), 11)), x, py + 142, 1, t.label, Align::Center);
        if (winner) g.icon(IconId::Crown, x - 5.f, py + 160.f, t.accent);
    };
    const bool mineWins = resultSet_ && result_ == Result::Win, theirsWin = resultSet_ && result_ == Result::Loss;
    column(lx, L.tr("battle.you"), score_, maxLevel_, mineWins);
    column(rx, opp_->name(), opp_->score(), opp_->maxLevel(), theirsWin);
    g.rect(px + 14, py + 186, pw - 28, 1, t.panelLo);
    g.text(L.tr(modName(mod_)), cx, py + 200, 1, t.dim, Align::Center);
    float y = py + 222;
    if (resultSet_) {
        if (!opp_->isLan()) {
            if (ui.button(L.tr("battle.rematch"), px + 20, y, pw - 40, 26, true, true, BtnStyle::Primary)) rematch(c);
            y += 34;
        }
        if (ui.button(L.tr("battle.menu"), px + 20, y, pw - 40, 26, true, true, opp_->isLan() ? BtnStyle::Primary : BtnStyle::Normal)) c.go(SceneId::Battle);
        y += 34;
        if (ui.button(L.tr("over.menu"), px + 20, y, pw - 40, 26) || c.in.esc) c.go(SceneId::Menu);
    } else {
        if (ui.button(L.tr("pause.quit"), px + 20, y + 20, pw - 40, 26)) c.go(SceneId::Battle);
    }
}

}  // namespace liq
