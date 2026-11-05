// "Additional features": run codes, player card and name, save backup / restore, authors.
#include <algorithm>
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

std::string cleanName(const std::string& s) {
    std::string o;
    for (char ch : s)
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == ' ' || ch == '_' || ch == '-')
            o += ch;
    return o;
}

class ExtrasScene final : public Scene {
public:
    ~ExtrasScene() override {
        if (editing_) SDL_StopTextInput();
    }

    void frame(Ctx& c, float dt) override {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        g.drawBackdrop(t, c.save.reduceMotion ? 0.f : c.time);
        g.text(L.tr("extras.title"), cfg::kW / 2.f, 28, 2, t.accent, Align::Center, TextFx::Outline, t.ink);

        if (editing_) {
            edit(c);
            return;
        }

        struct Row { const char* head; const char* desc; int colour; };
        const Row rows[] = {{"menu.runcode", "extras.runcode.desc", 2},  {"extras.card", "extras.card.desc", 5},
                            {"extras.name", "extras.name.desc", 9},       {"extras.backup", "extras.backup.desc", 7},
                            {"extras.restore", "extras.restore.desc", 4}, {"menu.authors", "extras.authors.desc", 10},
                            {"menu.quit", "extras.quit.desc", 0}};
        const int n = platform::isMobile() ? 6 : 7;
        for (int i = 0; i < n; ++i) {
            const float y = 54.f + float(i) * 50.f;
            std::string desc = L.tr(rows[i].desc);
            if (i == 2) desc = c.save.playerName.empty() ? L.tr("battle.you") : c.save.playerName;
            if (!card(c, y, 44, L.tr(rows[i].head), desc, rows[i].colour)) continue;
            switch (i) {
                case 0: runCode(c); break;
                case 1: c.go(SceneId::Card); break;
                case 2:
                    editing_ = true;
                    name_ = c.save.playerName;
                    SDL_StartTextInput();
                    break;
                case 3: backup(c); break;
                case 4: restore(c); break;
                case 5: c.go(SceneId::Credits); break;
                case 6: c.quit = true; break;
            }
        }
        if (msgT_ > 0.f) {
            msgT_ -= dt;
            g.text(msg_, cfg::kW / 2.f, 408.f, 1, msgBad_ ? t.danger : t.accent, Align::Center, TextFx::Outline, t.ink);
        }
        if (ui.button(L.tr("common.back"), 22, 430, 226, 26) || c.in.esc) c.go(SceneId::Menu);
    }

private:
    bool card(Ctx& c, float y, float h, const std::string& head, const std::string& desc, int colour) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        const bool rtl = c.loc.rtl();
        const float x = 22.f, w = 226.f;
        const bool hit = c.ui.button("", x, y, w, h);
        g.rect(rtl ? x + w - 7 : x + 2, y + 3, 5, h - 6, t.blob[colour % 12]);
        const Align la = c.ui.start();
        g.text(head, rtl ? x + w - 16 : x + 16, y + 13, 1, t.text, la);
        g.text(desc, rtl ? x + w - 16 : x + 16, y + 30, 1, t.dim, la);
        return hit;
    }

    void say(Ctx& c, const char* key, bool bad = false) {
        msg_ = c.loc.tr(key);
        msgBad_ = bad;
        msgT_ = 2.8f;
    }

    void runCode(Ctx& c) {  // watch a friend's run from the clipboard
        char* txt = SDL_GetClipboardText();
        Replay r;
        if (txt && Replay::fromCode(txt, r) && !r.empty()) {
            c.pendingReplay = r;
            c.hasPendingReplay = true;
            c.go(SceneId::Game);
        } else {
            say(c, "menu.badcode", true);
        }
        if (txt) SDL_free(txt);
    }

    void backup(Ctx& c) {
        c.save.store();
        const std::string src = platform::prefPath() + "save.txt";
        const auto d = platform::readFile(src);
        const std::string text(d.begin(), d.end());
        if (text.empty()) {
            say(c, "extras.fail", true);
            return;
        }
        SDL_SetClipboardText(text.c_str());
        platform::writeFile(platform::prefPath() + "liq2048-backup.txt", text);
        say(c, "extras.backup.ok");
    }

    void restore(Ctx& c) {
        char* clip = SDL_GetClipboardText();
        const std::string text = clip ? clip : "";
        if (clip) SDL_free(clip);
        // Only accept something that looks like a Liq2048 save: key=value lines with known keys, of sane size.
        const bool ok = text.size() > 40 && text.size() < 65536 && text.find("best0=") != std::string::npos &&
                        text.find("gamesPlayed=") != std::string::npos;
        if (!ok) {
            say(c, "extras.restore.bad", true);
            return;
        }
        const std::string path = platform::prefPath() + "save.txt";
        const auto old = platform::readFile(path);
        platform::writeFile(platform::prefPath() + "save-before-restore.txt", std::string(old.begin(), old.end()));
        platform::writeFile(path, text);
        Save fresh;
        fresh.load();
        c.save = fresh;
        c.save.store();
        c.applySettings();
        say(c, "extras.restore.ok");
    }

    void edit(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        g.text(L.tr("extras.name"), cfg::kW / 2.f, 70, 1, t.dim, Align::Center);
        c.ui.panel(22, 90, 226, 34);
        const std::string shown = name_ + ((int(c.time * 2.f) % 2 == 0) ? "_" : " ");
        g.text(shown, cfg::kW / 2.f, 107, 1, t.text, Align::Center);
        g.text(L.tr("extras.name.hint"), cfg::kW / 2.f, 140, 1, t.dim, Align::Center);
        name_ += cleanName(c.in.text);
        if (name_.size() > 12) name_.resize(12);
        if (c.in.backspace && !name_.empty()) name_.pop_back();
        const bool done = c.ui.button(L.tr("extras.done"), 148, 430, 100, 26, true, true, BtnStyle::Primary) || c.in.confirm;
        if (done) {
            while (!name_.empty() && name_.back() == ' ') name_.pop_back();
            c.save.playerName = name_;
            c.save.store();
            editing_ = false;
            SDL_StopTextInput();
        }
        if (c.ui.button(L.tr("battle.cancel"), 22, 430, 100, 26) || c.in.esc) {
            editing_ = false;
            SDL_StopTextInput();
        }
    }

    bool editing_ = false, msgBad_ = false;
    std::string name_, msg_;
    float msgT_ = 0.f;
};

// A shareable card of the player's lifetime stats, saved as a PNG.
class CardScene final : public Scene {
public:
    void frame(Ctx& c, float dt) override {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        const Save& s = c.save;
        g.drawBackdrop(t, 0.f);  // still backdrop so the picture is clean
        const float cx = cfg::kW / 2.f;
        g.text("LIQ2048", cx, 26, 2, t.accent, Align::Center, TextFx::Outline, t.ink);
        c.ui.panel(14, 44, 242, 372);
        const std::string name = s.playerName.empty() ? L.tr("battle.you") : s.playerName;
        g.text(name, cx, 66, 2, t.text, Align::Center, TextFx::Outline, t.ink);

        // The player's best droplet, alive.
        std::vector<BlobSprite> sp;
        const int lv = std::min(std::max(s.highestLevel, 0), 11);
        const float T = capturing_ ? 0.f : c.time;
        sp.push_back(plainSprite(cx, 122.f + (capturing_ ? 0.f : std::sin(T * 1.4f) * 2.f), std::min(lv, 5), 0.06f, T * 0.4f));
        c.raster.clear();
        c.raster.drawField(sp, t);
        c.raster.drawLabels(sp, t);
        g.raster(c.raster);

        struct Stat { const char* key; int v; };
        const Stat st[] = {{"card.games", s.gamesPlayed},   {"card.drops", s.totalDrops}, {"card.merges", s.totalMerges},
                           {"card.chain", s.longestChain},  {"card.puzzles", s.puzzlesSolved()}, {"card.streak", s.streak}};
        const bool rtl = L.rtl();
        for (int i = 0; i < 6; ++i) {
            const float y = 170.f + float(i) * 17.f;
            g.text(L.tr(st[i].key), rtl ? 244.f : 26.f, y, 1, t.dim, rtl ? Align::Right : Align::Left);
            g.number(std::to_string(st[i].v), rtl ? 26.f : 244.f, y, 1, t.text, rtl ? Align::Left : Align::Right);
        }
        g.rect(26, 276, 218, 1, t.panelLo);
        struct Best { const char* key; int v; };
        const Best bs[] = {{"mode.classic", s.best[int(ModeId::Classic)]}, {"mode.timed", s.best[int(ModeId::Timed)]},
                           {"mode.rain", s.best[int(ModeId::Rain)]},       {"mode.tide", s.best[int(ModeId::Tide)]}};
        for (int i = 0; i < 4; ++i) {
            const float y = 288.f + float(i) * 17.f;
            g.text(L.tr(bs[i].key), rtl ? 244.f : 26.f, y, 1, t.dim, rtl ? Align::Right : Align::Left);
            g.number(std::to_string(bs[i].v), rtl ? 26.f : 244.f, y, 1, t.accent, rtl ? Align::Left : Align::Right);
        }
        g.text(L.tr("mode.battle"), rtl ? 244.f : 26.f, 362, 1, t.dim, rtl ? Align::Right : Align::Left);
        g.number(std::to_string(s.battleWins) + " - " + std::to_string(s.battleLosses) + " - " + std::to_string(s.battleDraws),
                 rtl ? 26.f : 244.f, 362, 1, t.accent, rtl ? Align::Left : Align::Right);
        g.number("V" LIQ_VERSION, cx, 400, 1, t.dim, Align::Center);

        if (capturing_) {  // the frame without buttons is what gets saved
            capturing_ = false;
            const std::string path = platform::prefPath() + "player-card.png";
            const bool ok = g.capturePng(path);
            msg_ = ok ? L.tr("card.saved") : L.tr("extras.fail");
            msgBad_ = !ok;
            msgT_ = 3.f;
            if (ok) {
                const std::string txt = "Liq2048 V" LIQ_VERSION " - " + name + ": " + std::to_string(s.gamesPlayed) + " games, best droplet " +
                                        std::to_string(2 << std::min(s.highestLevel, 10)) + ", longest chain x" + std::to_string(s.longestChain);
                SDL_SetClipboardText(txt.c_str());
            }
            return;
        }
        if (msgT_ > 0.f) {
            msgT_ -= dt;
            g.text(msg_, cx, 424, 1, msgBad_ ? t.danger : t.accent, Align::Center, TextFx::Outline, t.ink);
        }
        if (c.ui.button(L.tr("card.save"), 148, 440, 100, 26, true, true, BtnStyle::Primary)) {
            capturing_ = true;
            msgT_ = 0.f;
        }
        if (c.ui.button(L.tr("common.back"), 22, 440, 100, 26) || c.in.esc) c.go(SceneId::Extras);
    }

private:
    bool capturing_ = false, msgBad_ = false;
    std::string msg_;
    float msgT_ = 0.f;
};

}  // namespace

std::unique_ptr<Scene> makeExtrasScene() { return std::make_unique<ExtrasScene>(); }
std::unique_ptr<Scene> makeCardScene() { return std::make_unique<CardScene>(); }

}  // namespace liq
