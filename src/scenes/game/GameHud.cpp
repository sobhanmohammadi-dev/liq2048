// GameScene HUD: score header, pause/undo buttons, history strip, hint banner and toast.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "scenes/game/GameScene.hpp"
#include "ui/widgets/Widgets.hpp"

namespace liq {

// Controls while watching a run code: pause / play, speed, and how far the replay is.
void GameScene::replayBar(Ctx& c) {
    Gfx& g = c.gfx;
    Ui& ui = c.ui;
    const Theme& t = *c.theme;
    const float y = 452.f;
    if (ui.iconButton(replayPaused_ ? IconId::Play : IconId::Pause, 22, y, 30, 24, true, true)) replayPaused_ = !replayPaused_;
    static const int speeds[3] = {1, 2, 4};
    if (ui.button(std::to_string(replaySpeed_) + "x", 58, y, 36, 24)) {
        int k = 0;
        while (k < 2 && speeds[k] != replaySpeed_) ++k;
        replaySpeed_ = speeds[(k + 1) % 3];
    }
    const uint32_t total = rec_.endStep ? rec_.endStep : (rec_.drops.empty() ? 1u : rec_.drops.back().step + 240u);
    const float f = clampv(float(simStep_) / float(std::max(1u, total)), 0.f, 1.f);
    const float bx = 102.f, bw = 146.f;
    widgets::meter(g, t, bx - 1, y + 8, bw + 2, 10, f);
    if (c.in.pressed && c.in.mx >= 22 && c.in.mx <= 94 && c.in.my >= y && c.in.my <= y + 24) c.audio.play(Sfx::Click);
    if (rec_.physics != cfg::kPhysicsVersion && !olderShown_) {
        olderShown_ = true;
        toast_ = c.loc.tr("replay.older");
        toastT_ = 4.f;
    }
}

void GameScene::drawNextPanel(Ctx& c) {
    const Theme& t = *c.theme;
    const float nx = c.ui.mx(8, 44);
    c.ui.panel(nx, 8, 44, 44);
    c.gfx.text(c.loc.tr("hud.next"), nx + 22, 15, 1, t.dim, Align::Center);
}

void GameScene::drawHud(Ctx& c) {
    Gfx& g = c.gfx;
    const Theme& t = *c.theme;
    Ui& ui = c.ui;
    Loc& L = c.loc;
    const bool play = st_ == St::Playing;

    // Score (counts up) + best. In Battle: the opponent's score under yours (or yours under theirs while watching).
    const bool vw = viewing();
    g.number(std::to_string(vw ? opp_->score() : int(std::lround(dispScore_))), cfg::kW / 2.f, 24, 3, vw ? t.accent : t.text, Align::Center,
             TextFx::Outline, t.ink);
    if (battle_) {
        const std::string line = vw ? L.tr("battle.you") + " " + std::to_string(score_) : "VS " + opp_->name() + " " + std::to_string(opp_->score());
        const bool behind = !vw && opp_->score() > score_;
        g.text(line, cfg::kW / 2.f, 48, 1, behind ? t.danger : t.dim, Align::Center);
    } else if (hot_) {  // two scores, the player on turn highlighted
        for (int p = 0; p < 2; ++p) {
            const bool on = turn_ == p && st_ == St::Playing;
            const float x = p ? 190.f : 80.f;
            g.text((on ? "> " : "") + L.tr(p ? "hot.p2" : "hot.p1"), x, 48, 1, on ? t.accent : t.dim, Align::Center);
            g.number(std::to_string(hs_[p]), x, 60, 1, on ? t.text : t.dim, Align::Center);
        }
    } else if (mid_ != ModeId::Puzzle) {
        std::string best = L.tr("hud.best") + " " + std::to_string(std::max(bestFor(c), score_));
        const std::vector<int>& gh = c.save.ghost[int(mid_)];
        if (ghostMode() && !replaying_ && !gh.empty() && st_ != St::Over) {  // pace against your best run
            const int d = score_ - gh[std::min(size_t(drops_), gh.size() - 1)];
            best += "  " + L.tr("hud.ghost") + " " + (d >= 0 ? "+" : "-") + std::to_string(std::abs(d));
        }
        const float bw = float(g.textW(best, 1));
        g.icon(IconId::Crown, cfg::kW / 2.f - bw / 2.f - 13.f, 45.f, t.accent);
        g.text(best, cfg::kW / 2.f + 5.f, 48, 1, t.dim, Align::Center);
    }

    // Pause.
    const float px = ui.mx(238, 24);
    if (ui.iconButton(IconId::Pause, px, 8, 24, 24, play, false)) {
        st_ = St::Paused;
        ui.resetFocus();
    }

    // Undo (Classic and Zen): icon plus remaining charges.
    if (undoAllowed()) {
        const float ux = ui.mx(8, 44);
        const bool can = play && snap_.valid && undos_ > 0;
        if (ui.button("", ux, 56, 44, 22, can, can)) doUndo(c);
        g.icon(IconId::Undo, ux + 8, 64.f, can ? t.text : t.dim);
        g.number(std::to_string(undos_), ux + 32.f, 67, 1, can ? t.text : t.dim, Align::Center);
    }

    // Mode, status, segmented progress bar.
    const Stats s = stats();
    if (vw) g.text(opp_->name() + " - " + L.tr("battle.viewing"), cfg::kW / 2.f, 64, 1, t.danger, Align::Center);
    else g.text(L.tr(mode_->nameKey()), cfg::kW / 2.f, 64, 1, t.accent, Align::Center);
    const std::string status = mode_->status(L, s);
    if (!status.empty()) g.text(status, cfg::kW / 2.f, 77, 1, t.text, Align::Center);
    const float pg = mode_->progress(s);
    if (pg >= 0.f) {
        widgets::segments(g, t, 86, 20, pg);
    }
}

// Bottom strip: droplet levels reached so far.
void GameScene::drawHistory(Ctx& c) { widgets::levelStrip(c.gfx, *c.theme, 468.f, maxLevel_); }

void GameScene::drawHint(Ctx& c, bool paused) {
    if (hint_.empty() || st_ == St::Tutorial || st_ == St::Over) return;
    if (hintT_ > 0.f && !paused) {
        hintT_ -= 1.f / 60.f;
        if (hintT_ <= 0.f) {
            hint_.clear();
            return;
        }
    }
    widgets::banner(c.gfx, *c.theme, 190.f, 196.f, hint_);
}

void GameScene::drawToast(Ctx& c) {
    if (toastT_ > 0.f) widgets::toast(c.gfx, *c.theme, toast_, toastT_, 3.f);
}

}  // namespace liq
