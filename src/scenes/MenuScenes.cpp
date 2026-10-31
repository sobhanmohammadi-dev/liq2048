// Menu, mode selection, progress and standalone settings scenes.
#include <string>

#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "scenes/Scenes.hpp"

namespace liq {

namespace {

class MenuScene final : public Scene {
public:
    void frame(Ctx& c, float) override {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        g.clear(t.bg);
        drawStars(c);
        g.bitmap("LIQ2048", 180, 110, 7, t.accent, Align::Center);
        // Decorative flat blobs.
        for (int i = 0; i < 6; ++i) {
            float r = 8.f + i * 3.f;
            g.ellipse(60 + i * 48.f, 165.f, r + 2, r + 2, mix(t.blob[i], rgb(0x000000), 0.55f));
            g.ellipse(60 + i * 48.f, 165.f, r, r, t.blob[i]);
        }

        float y = 220;
        if (ui.button(L.tr("menu.play"), 80, y, 200, 44)) c.go(SceneId::ModeSelect);
        y += 56;
        if (ui.button(L.tr("menu.progress"), 80, y, 200, 44)) c.go(SceneId::Progress);
        y += 56;
        if (ui.button(L.tr("menu.settings"), 80, y, 200, 44)) c.go(SceneId::Settings);
        y += 56;
        if (!platform::isMobile() && ui.button(L.tr("menu.quit"), 80, y, 200, 44)) c.quit = true;

        g.text(L.fmt("menu.streak", std::to_string(c.save.streak)), 180, 500, 1, t.dim, Align::Center);
        langToggle(c, 310, 10);
        if (c.in.esc && !platform::isMobile()) c.quit = true;
    }
};

class ModeSelectScene final : public Scene {
public:
    void frame(Ctx& c, float) override {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        g.clear(t.bg);
        drawStars(c);
        g.text(L.tr("select.title"), 180, 50, 3, t.accent, Align::Center);
        static const char* names[kModeCount] = {"mode.classic", "mode.timed", "mode.target", "mode.daily", "mode.zen"};
        static const char* descs[kModeCount] = {"mode.classic.desc", "mode.timed.desc", "mode.target.desc",
                                                "mode.daily.desc", "mode.zen.desc"};
        const bool rtl = L.rtl();
        for (int i = 0; i < kModeCount; ++i) {
            float y = 90.f + i * 78.f;
            if (ui.button("", 30, y, 300, 66)) {
                c.mode = ModeId(i);
                c.go(SceneId::Game);
            }
            float lx = rtl ? 320.f : 40.f, rx = rtl ? 40.f : 320.f;
            Align la = ui.start(), ra = rtl ? Align::Left : Align::Right;
            g.text(L.tr(names[i]), lx, y + 20, 2, t.text, la);
            g.text(L.tr(descs[i]), lx, y + 46, 1, t.dim, la);
            int best = i == int(ModeId::Daily) ? (c.save.dailyDay == platform::dayNumber() ? c.save.dailyToday : 0)
                                               : c.save.best[i];
            if (best > 0) g.bitmap(std::to_string(best), rx, y + 20, 2, t.accent, ra);
        }
        if (ui.button(L.tr("common.back"), 80, 500, 200, 40) || c.in.esc) c.go(SceneId::Menu);
        langToggle(c, 310, 10);
    }
};

class ProgressScene final : public Scene {
public:
    void frame(Ctx& c, float) override {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        g.clear(t.bg);
        drawStars(c);
        g.text(L.tr("progress.title"), 180, 40, 3, t.accent, Align::Center);
        g.text(L.fmt("menu.streak", std::to_string(c.save.streak)), 180, 72, 1, t.dim, Align::Center);

        const bool rtl = L.rtl();
        float lx = rtl ? 330.f : 30.f, rx = rtl ? 30.f : 330.f;
        Align la = ui.start(), ra = rtl ? Align::Left : Align::Right;
        static const char* names[kModeCount] = {"mode.classic", "mode.timed", "mode.target", "mode.daily", "mode.zen"};

        g.text(L.tr("progress.bests"), lx, 100, 2, t.accent, la);
        for (int i = 0; i < kModeCount; ++i) {
            float y = 126.f + i * 22.f;
            g.text(L.tr(names[i]), lx, y, 1, t.text, la);
            g.bitmap(std::to_string(c.save.best[i]), rx, y, 2, t.text, ra);
        }

        g.text(L.tr("progress.achievements"), lx, 256, 2, t.accent, la);
        for (int i = 0; i < kAchCount; ++i) {
            float y = 284.f + i * 22.f;
            bool got = c.save.has(i);
            float bx = rtl ? 330.f - 12.f : 30.f;
            g.rect(bx, y - 6, 12, 12, got ? t.accent : t.panel);
            g.border(bx, y - 6, 12, 12, 2, got ? t.accent : t.dim);
            g.text(L.tr(achKey(i)), rtl ? 310.f : 52.f, y, 1, got ? t.text : t.dim, la);
        }
        if (ui.button(L.tr("common.back"), 80, 540, 200, 40) || c.in.esc) c.go(SceneId::Menu);
    }
};

class SettingsScene final : public Scene {
public:
    void frame(Ctx& c, float) override {
        c.gfx.clear(c.theme->bg);
        drawStars(c);
        if (drawSettings(c, 70) || c.in.esc) c.go(SceneId::Menu);
    }
};

}  // namespace

std::unique_ptr<Scene> makeMenuScene() { return std::make_unique<MenuScene>(); }
std::unique_ptr<Scene> makeModeSelectScene() { return std::make_unique<ModeSelectScene>(); }
std::unique_ptr<Scene> makeProgressScene() { return std::make_unique<ProgressScene>(); }
std::unique_ptr<Scene> makeSettingsScene() { return std::make_unique<SettingsScene>(); }

std::unique_ptr<Scene> makeScene(SceneId id, Ctx& c) {
    switch (id) {
        case SceneId::ModeSelect: return makeModeSelectScene();
        case SceneId::Progress: return makeProgressScene();
        case SceneId::Settings: return makeSettingsScene();
        case SceneId::Game: return makeGameScene(c);
        default: return makeMenuScene();
    }
}

}  // namespace liq
