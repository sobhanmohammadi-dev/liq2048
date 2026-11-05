// Battle menus: choose bot or human, bot difficulty, and the LAN host / browse / join flow.
#include <cmath>
#include <ctime>
#include <string>

#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "scenes/Scenes.hpp"

namespace liq {

namespace {

constexpr int kDurations[4] = {60, 90, 120, 180};

int durIndex(int d) {
    for (int i = 0; i < 4; ++i)
        if (kDurations[i] == d) return i;
    return 2;
}

const char* modKey(int m) {
    static const char* k[5] = {"mod.none", "mod.lowg", "mod.heavy", "mod.thick", "mod.bonus"};
    return k[m < 0 || m > 4 ? 0 : m];
}

uint32_t freshSeed() {
    static uint32_t n = 0;
    return uint32_t(std::time(nullptr)) * 2654435761u + SDL_GetTicks() * 40503u + (++n) * 977u;
}

std::string durText(Loc& L, int s) { return L.fmt("battle.secs", std::to_string(s)); }

class BattleScene final : public Scene {
public:
    ~BattleScene() override { dropLobbies(); }

    void frame(Ctx& c, float dt) override {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        g.drawBackdrop(t, c.save.reduceMotion ? 0.f : c.time);
        std::vector<BlobSprite> sp;
        switch (page_) {
            case Page::Home: home(c, sp); break;
            case Page::Bot: bot(c); break;
            case Page::Human: human(c); break;
            case Page::HostSetup: hostSetup(c); break;
            case Page::HostWait: hostWait(c, dt); break;
            case Page::Browse: browse(c, dt); break;
            case Page::JoinWait: joinWait(c, dt); break;
        }
        if (!sp.empty()) {
            c.raster.clear();
            c.raster.drawField(sp, t);
            c.raster.drawLabels(sp, t);
            g.raster(c.raster);
        }
        if (!c.hasNext && c.in.esc) back(c);
    }

private:
    enum class Page { Home, Bot, Human, HostSetup, HostWait, Browse, JoinWait };

    void title(Ctx& c, const std::string& s, const std::string& sub = "") {
        const Theme& t = *c.theme;
        c.gfx.text(s, cfg::kW / 2.f, 30, 2, t.accent, Align::Center, TextFx::Outline, t.ink);
        if (!sub.empty()) c.gfx.text(sub, cfg::kW / 2.f, 54, 1, t.dim, Align::Center);
    }

    // A tappable card with a coloured spine, a heading and a description.
    bool card(Ctx& c, float y, float h, const std::string& head, const std::string& desc, int colour, bool enabled = true) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Ui& ui = c.ui;
        const bool rtl = c.loc.rtl();
        const float x = 22.f, w = 226.f;
        const bool hit = ui.button("", x, y, w, h, enabled);
        g.rect(rtl ? x + w - 7 : x + 2, y + 3, 5, h - 6, t.blob[colour % 12]);
        const Align la = ui.start();
        g.text(head, rtl ? x + w - 16 : x + 16, y + 15, 1, enabled ? t.text : t.dim, la);
        ui.paragraph(desc, rtl ? x + 12 : x + 16, y + 33, w - 28, 1, t.dim, la);
        return hit;
    }

    void row(Ctx& c, float y, const std::string& label, const std::string& value, int& delta) {
        const Theme& t = *c.theme;
        const bool rtl = c.loc.rtl();
        c.gfx.text(label, rtl ? 244.f : 26.f, y + 10, 1, t.dim, c.ui.start());
        const float cw = 112.f, cx = c.ui.mx(132.f, cw);
        delta = c.ui.stepper(cx, y, cw, 20, value);
    }

    void backBtn(Ctx& c, Page to, bool leaveCleanup = false) {
        if (c.ui.button(c.loc.tr("common.back"), 22, 430, 100, 26)) {
            if (leaveCleanup) dropLobbies();
            page_ = to;
        }
    }

    void back(Ctx& c) {
        switch (page_) {
            case Page::Home: c.go(SceneId::Menu); break;
            case Page::Bot:
            case Page::Human: page_ = Page::Home; break;
            case Page::HostSetup:
            case Page::Browse: dropLobbies(); page_ = Page::Human; break;
            case Page::HostWait: dropLobbies(); page_ = Page::HostSetup; break;
            case Page::JoinWait: dropLobbies(); page_ = Page::Browse; startBrowse(); break;
        }
    }

    void dropLobbies() {
        if (host_) host_->cancel();
        if (join_) join_->cancel();
        if (browser_) browser_->stop();
        host_.reset();
        join_.reset();
        browser_.reset();
    }

    void startBrowse() {
        browser_ = std::make_unique<battle::Browser>();
        browseOk_ = browser_->start();
    }

    // ---------------- pages ----------------
    void home(Ctx& c, std::vector<BlobSprite>& sp) {
        Loc& L = c.loc;
        const Theme& t = *c.theme;
        title(c, L.tr("mode.battle"), L.tr("battle.tag"));
        for (int i = 0; i < 2; ++i) {  // two droplets squaring off
            const float s = i ? 1.f : -1.f, r = World::radiusFor(4 + i * 2);
            const float push = 6.f + std::sin(c.time * 2.f) * 3.f;
            sp.push_back(plainSprite(cfg::kW / 2.f + s * (r + 6.f - push * 0.3f), 112.f, 4 + i * 2,
                                     0.08f + 0.04f * std::sin(c.time * 3.f + float(i)), c.time + float(i)));
        }
        if (card(c, 150, 62, L.tr("battle.vsbot"), L.tr("battle.vsbot.desc"), 3)) page_ = Page::Bot;
        if (card(c, 220, 62, L.tr("battle.vshuman"), L.tr("battle.vshuman.desc"), 7)) page_ = Page::Human;
        c.gfx.text(L.fmt("battle.record", std::to_string(c.save.battleWins) + " - " + std::to_string(c.save.battleLosses) +
                                              " - " + std::to_string(c.save.battleDraws)),
                   cfg::kW / 2.f, 304, 1, t.dim, Align::Center);
        if (c.ui.button(L.tr("common.back"), 22, 430, 100, 26)) c.go(SceneId::Menu);
    }

    void bot(Ctx& c) {
        Loc& L = c.loc;
        Save& s = c.save;
        title(c, L.tr("battle.vsbot"), L.tr("battle.pickdiff"));
        static const char* dk[4] = {"battle.diff.easy", "battle.diff.medium", "battle.diff.hard", "battle.diff.adaptive"};
        static const char* dd[4] = {"battle.diff.easy.desc", "battle.diff.medium.desc", "battle.diff.hard.desc",
                                    "battle.diff.adaptive.desc"};
        for (int i = 0; i < 4; ++i) {
            const float y = 72.f + float(i) * 56.f;
            const bool sel = s.battleDifficulty == i;
            if (card(c, y, 50, (sel ? "> " : "") + L.tr(dk[i]), L.tr(dd[i]), 2 + i * 2)) {
                s.battleDifficulty = i;
                s.store();
            }
        }
        int d = 0;
        row(c, 300, L.tr("battle.time"), durText(L, s.battleDuration), d);
        if (d) {
            s.battleDuration = kDurations[(durIndex(s.battleDuration) + d + 4) % 4];
            s.store();
        }
        row(c, 326, L.tr("mod.label"), L.tr(modKey(s.battleModifier)), d);
        if (d) {
            s.battleModifier = (s.battleModifier + d + 5) % 5;
            s.store();
        }
        if (c.ui.button(L.tr("battle.fight"), 148, 430, 100, 26, true, true, BtnStyle::Primary)) {
            battle::MatchConfig m;
            m.seed = freshSeed();
            m.modifier = s.battleModifier;
            m.duration = s.battleDuration;
            m.lan = false;
            m.difficulty = battle::Difficulty(s.battleDifficulty);
            m.myName = L.tr("battle.you");
            m.oppName = L.tr(dk[s.battleDifficulty]);
            c.battle.cfg = m;
            c.battle.link.reset();
            c.mode = ModeId::Battle;
            c.go(SceneId::Game);
        }
        backBtn(c, Page::Home);
    }

    void human(Ctx& c) {
        Loc& L = c.loc;
        title(c, L.tr("battle.vshuman"), L.tr("battle.lan.note"));
        if (card(c, 84, 62, L.tr("battle.host"), L.tr("battle.host.desc"), 5)) page_ = Page::HostSetup;
        if (card(c, 154, 62, L.tr("battle.browse"), L.tr("battle.browse.desc"), 9)) {
            startBrowse();
            page_ = Page::Browse;
        }
        c.gfx.text(L.tr("battle.sameWifi"), cfg::kW / 2.f, 250, 1, c.theme->dim, Align::Center);
        backBtn(c, Page::Home);
    }

    void hostSetup(Ctx& c) {
        Loc& L = c.loc;
        Save& s = c.save;
        title(c, L.tr("battle.host"), L.tr("battle.setup"));
        int d = 0;
        row(c, 100, L.tr("battle.time"), durText(L, s.battleDuration), d);
        if (d) {
            s.battleDuration = kDurations[(durIndex(s.battleDuration) + d + 4) % 4];
            s.store();
        }
        row(c, 130, L.tr("mod.label"), L.tr(modKey(s.battleModifier)), d);
        if (d) {
            s.battleModifier = (s.battleModifier + d + 5) % 5;
            s.store();
        }
        if (c.ui.button(L.tr("battle.create"), 148, 430, 100, 26, true, true, BtnStyle::Primary)) {
            battle::MatchConfig m;
            m.seed = freshSeed();
            m.modifier = s.battleModifier;
            m.duration = s.battleDuration;
            m.lan = true;
            m.myName = L.tr("battle.you");
            host_ = std::make_unique<battle::HostLobby>();
            if (host_->start(m)) {
                page_ = Page::HostWait;
            } else {
                host_.reset();
                err_ = 3.f;
            }
        }
        if (err_ > 0.f) {
            err_ -= 1.f / 60.f;
            c.gfx.text(L.tr("battle.neterr"), cfg::kW / 2.f, 400, 1, c.theme->danger, Align::Center);
        }
        backBtn(c, Page::Human);
    }

    void hostWait(Ctx& c, float dt) {
        Loc& L = c.loc;
        const Theme& t = *c.theme;
        Gfx& g = c.gfx;
        if (!host_) { page_ = Page::HostSetup; return; }
        host_->update(dt);
        title(c, L.tr("battle.lobby"));
        const std::string ip = net::localIp();
        g.text(L.tr("battle.yourip"), cfg::kW / 2.f, 90, 1, t.dim, Align::Center);
        g.number(ip + ":" + std::to_string(host_->port()), cfg::kW / 2.f, 108, 1, t.text, Align::Center);
        g.text(durText(L, host_->config().duration) + "  -  " + L.tr(modKey(host_->config().modifier)), cfg::kW / 2.f,
               132, 1, t.dim, Align::Center);
        c.ui.panel(22, 160, 226, 110);
        if (host_->failed()) {
            g.text(L.tr("battle.neterr"), cfg::kW / 2.f, 212, 1, t.danger, Align::Center);
        } else if (host_->hasGuest()) {
            g.text(L.tr("battle.joined"), cfg::kW / 2.f, 190, 1, t.dim, Align::Center);
            g.text(host_->guestName(), cfg::kW / 2.f, 214, 2, t.accent, Align::Center, TextFx::Outline, t.ink);
        } else {
            const int dots = int(c.time * 2.f) % 4;
            g.text(L.tr("battle.waitplayer") + std::string(size_t(dots), '.'), cfg::kW / 2.f, 212, 1, t.text, Align::Center);
        }
        const bool ready = host_->hasGuest() && !host_->failed();
        if (c.ui.button(L.tr("battle.start"), 148, 430, 100, 26, ready, ready, BtnStyle::Primary)) {
            auto link = host_->begin();
            if (link) {
                c.battle.cfg = link->cfg;
                c.battle.link = link;
                c.mode = ModeId::Battle;
                host_.reset();
                c.go(SceneId::Game);
                return;
            }
        }
        if (c.ui.button(L.tr("battle.cancel"), 22, 430, 100, 26)) {
            dropLobbies();
            page_ = Page::HostSetup;
        }
    }

    void browse(Ctx& c, float dt) {
        Loc& L = c.loc;
        const Theme& t = *c.theme;
        Gfx& g = c.gfx;
        title(c, L.tr("battle.browse"), L.tr("battle.browse.sub"));
        if (!browser_) startBrowse();
        browser_->update(dt);
        const auto games = browser_->games();
        int shown = 0;
        for (const auto& p : games) {
            if (shown >= 5) break;
            const bool open = p.info.players < 2;
            const std::string meta = durText(L, p.info.duration) + "  -  " + L.tr(modKey(p.info.modifier));
            if (card(c, 72.f + float(shown) * 60.f, 54, p.info.hostName, open ? meta : L.tr("battle.full"), 5, open) && open) {
                join_ = std::make_unique<battle::JoinLobby>();
                join_->start(p, L.tr("battle.you"));
                if (browser_) browser_->stop();
                browser_.reset();
                page_ = Page::JoinWait;
                return;
            }
            ++shown;
        }
        if (shown == 0) {
            const int dots = int(c.time * 2.f) % 4;
            g.text((browseOk_ ? L.tr("battle.searching") : L.tr("battle.neterr")) +
                       (browseOk_ ? std::string(size_t(dots), '.') : std::string()),
                   cfg::kW / 2.f, 200, 1, browseOk_ ? t.dim : t.danger, Align::Center);
        }
        backBtn(c, Page::Human, true);
    }

    void joinWait(Ctx& c, float dt) {
        Loc& L = c.loc;
        const Theme& t = *c.theme;
        Gfx& g = c.gfx;
        if (!join_) { page_ = Page::Browse; startBrowse(); return; }
        join_->update(dt);
        title(c, L.tr("battle.lobby"));
        c.ui.panel(22, 150, 226, 110);
        using S = battle::JoinLobby::State;
        const S st = join_->state();
        if (st == S::Failed) {
            g.text(L.tr("battle.joinfail"), cfg::kW / 2.f, 205, 1, t.danger, Align::Center);
        } else {
            const int dots = int(c.time * 2.f) % 4;
            g.text(L.tr(st == S::Connecting ? "battle.connecting" : "battle.waithost") + std::string(size_t(dots), '.'),
                   cfg::kW / 2.f, 205, 1, t.text, Align::Center);
        }
        if (st == S::Started) {
            auto link = join_->take();
            c.battle.cfg = link->cfg;
            c.battle.link = link;
            c.mode = ModeId::Battle;
            join_.reset();
            c.go(SceneId::Game);
            return;
        }
        if (c.ui.button(L.tr(st == S::Failed ? "common.back" : "battle.cancel"), 22, 430, 100, 26)) {
            dropLobbies();
            page_ = Page::Browse;
            startBrowse();
        }
    }

    Page page_ = Page::Home;
    std::unique_ptr<battle::HostLobby> host_;
    std::unique_ptr<battle::JoinLobby> join_;
    std::unique_ptr<battle::Browser> browser_;
    bool browseOk_ = true;
    float err_ = 0.f;
};

}  // namespace

std::unique_ptr<Scene> makeBattleScene() { return std::make_unique<BattleScene>(); }

}  // namespace liq
