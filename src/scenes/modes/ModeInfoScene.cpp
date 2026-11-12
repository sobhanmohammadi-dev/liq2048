// Mode information page: opened from the mode list. It explains the mode, plays a looping clip of it (ModePreview),
// lets the player pick the modifier and starts the game.
//
//   [X]
//   +---------------------------------+
//   |            Title               |
//   |         description             |
//   |   +-------------------------+   |
//   |   |   preview (video clip)  |   |
//   |   +-------------------------+   |
//   |         record / best           |
//   |            Modifier             |
//   |        [<  value  >]            |
//   |            [Start]              |
//   +---------------------------------+
#include <algorithm>
#include <string>

#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "core/Util.hpp"
#include "game/Mode.hpp"
#include "game/ModeNames.hpp"
#include "game/Modifier.hpp"
#include "scenes/Scenes.hpp"
#include "scenes/modes/ModePreview.hpp"
#include "ui/widgets/Widgets.hpp"

namespace liq {

namespace {

// Which modifier the mode accepts.
enum class ModKind { Free, Battle, Weekly, None };

ModKind modKindOf(ModeId m) {
    switch (m) {
        case ModeId::Battle: return ModKind::Battle;
        case ModeId::Weekly: return ModKind::Weekly;
        case ModeId::Daily:
        case ModeId::Puzzle:
        case ModeId::BigBox: return ModKind::None;
        default: return ModKind::Free;
    }
}

class ModeInfoScene final : public Scene {
public:
    void frame(Ctx& c, float dt) override {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        const ModeId m = c.mode;
        const float cx = float(cfg::kW) / 2.f;
        const Color accent = t.blob[(int(m) * 5 + 3) % 12];

        g.drawBackdrop(t, c.save.reduceMotion ? 0.f : c.time);
        ui.panel(12, 40, 246, 424);

        // title (shrinks to the small font when the translated name is long) and a colour rule
        const std::string title = L.tr(modeNameKey(m));
        const int ts = g.textW(title, 2) <= 214 ? 2 : 1;
        g.text(title, cx, 62, ts, t.text, Align::Center, TextFx::Outline, t.ink);
        g.rect(cx - 22.f, 78, 44, 2, accent);
        ui.paragraph(L.tr(modeDescKey(m)), 28, 88, 214, 1, t.dim, Align::Center);

        // the clip
        preview_.draw(c, m, (float(cfg::kW) - ModePreview::kW) / 2.f, 130.f, c.save.reduceMotion ? 0.f : dt);

        // personal record
        std::string rec;
        if (m == ModeId::Battle) rec = std::to_string(c.save.battleWins) + " - " + std::to_string(c.save.battleLosses) + " - " + std::to_string(c.save.battleDraws);
        else if (m == ModeId::Puzzle) rec = std::to_string(c.save.puzzlesSolved());
        else if (m == ModeId::Daily) rec = std::to_string(c.save.dailyDay == platform::dayNumber() ? c.save.dailyToday : 0);
        else if (m == ModeId::Weekly) rec = std::to_string(c.save.weeklyWeek == weekOf(platform::dayNumber()) ? c.save.weeklyBest : 0);
        else rec = std::to_string(c.save.best[int(m)]);
        const bool battleEmpty = m == ModeId::Battle && c.save.battleWins + c.save.battleLosses + c.save.battleDraws == 0;
        if (rec != "0" && !battleEmpty) {
            const std::string cap = m == ModeId::Battle || m == ModeId::Puzzle ? "" : L.tr("hud.best") + " ";
            const float w = float(g.textW(cap, 1) + g.numberW(rec, 1));
            g.icon(IconId::Crown, cx - w / 2.f - 14.f, 316, t.accent);
            if (!cap.empty()) g.text(cap, cx - w / 2.f, 322, 1, t.dim, Align::Left);
            g.number(rec, cx + w / 2.f, 322, 1, t.accent, Align::Right);
        }

        // modifier
        const ModKind mk = modKindOf(m);
        const float cw = 190.f, mx = (float(cfg::kW) - cw) / 2.f;
        if (mk == ModKind::Free || mk == ModKind::Battle) {
            g.text(L.tr("mod.label"), cx, 346, 1, t.dim, Align::Center);
            int& sel = mk == ModKind::Free ? c.save.modifier : c.save.battleModifier;
            const int n = mk == ModKind::Free ? kModifierCount : kBattleModifiers;
            if (const int d = ui.stepper(mx, 358, cw, 22, L.tr(modifierKey(sel)))) {
                sel = (sel + d + n) % n;
                c.save.store();
            }
        } else if (mk == ModKind::Weekly) {
            g.text(L.tr("mod.label"), cx, 346, 1, t.dim, Align::Center);
            g.text(L.fmt("mod.rule", L.tr(modifierKey(weeklyRuleOf(weekOf(platform::dayNumber()))))), cx, 368, 1, t.text, Align::Center);
        }

        else g.text(L.tr("modeinfo.nomod"), cx, 358, 1, t.dim, Align::Center);

        // start / close
        if (!c.save.tutorialDone && c.save.coachStep == 2) widgets::coachRing(g, t, c.time, mx, 404, cw, 36, L.tr("coach.info"), false);
        if (ui.button(L.tr("menu.start"), mx, 404, cw, 36, true, true, BtnStyle::Primary) || c.in.confirm) {
            if (c.save.coachStep == 2) c.save.coachStep = 3;
            c.audio.play(Sfx::Click);
            c.go(sceneForMode(m));
        }
        if (ui.button("X", ui.mx(8.f, 36.f), 8, 36, 24) || c.in.esc) c.go(SceneId::ModeSelect);
    }

private:
    ModePreview preview_;
};

}  // namespace

SceneId sceneForMode(ModeId m) {
    switch (m) {
        case ModeId::Puzzle: return SceneId::Puzzle;
        case ModeId::Battle: return SceneId::Battle;
        case ModeId::BigBox: return SceneId::Party;
        default: return SceneId::Game;
    }
}

std::unique_ptr<Scene> makeModeInfoScene() { return std::make_unique<ModeInfoScene>(); }

}  // namespace liq
