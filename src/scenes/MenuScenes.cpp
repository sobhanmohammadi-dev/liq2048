// Menu, mode selection, progress and standalone settings scenes.
#include <cmath>
#include <string>

#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "core/Version.hpp"
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
    void frame(Ctx& c, float) override {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        g.drawBackdrop(t, c.time);
        drawWordmark(c, cfg::kW / 2.f, 70.f + std::sin(c.time * 1.6f) * 1.f, 5);
        g.text(L.tr("menu.tagline"), cfg::kW / 2.f, 106.f, 1, t.dim, Align::Center);
        drawLogoCluster(c, 160.f);

        const float bw = 150, bx = (cfg::kW - bw) / 2.f;
        float y = 232;
        if (ui.button(L.tr("menu.play"), bx, y, bw, 30, true, true, BtnStyle::Primary)) c.go(SceneId::ModeSelect);
        y += 40;
        if (ui.button(L.tr("menu.progress"), bx, y, bw, 30)) c.go(SceneId::Progress);
        y += 40;
        if (ui.button(L.tr("menu.settings"), bx, y, bw, 30)) c.go(SceneId::Settings);
        y += 40;
        if (!platform::isMobile() && ui.button(L.tr("menu.quit"), bx, y, bw, 30)) c.quit = true;

        g.icon(IconId::Crown, cfg::kW / 2.f - float(g.textW(L.fmt("menu.streak", std::to_string(c.save.streak)), 1)) / 2.f - 14.f,
               432.f, t.accent);
        g.text(L.fmt("menu.streak", std::to_string(c.save.streak)), cfg::kW / 2.f, 435.f, 1, t.dim, Align::Center);
        g.number("V" LIQ_VERSION, cfg::kW - 6.f, 470.f, 1, mix(t.dim, t.sky[3], 0.4f), Align::Right);
        langToggle(c, 242, 8);
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
        g.drawBackdrop(t, c.time);
        g.text(L.tr("select.title"), cfg::kW / 2.f, 28, 2, t.accent, Align::Center, TextFx::Outline, t.ink);
        static const char* names[kModeCount] = {"mode.classic", "mode.timed", "mode.target", "mode.daily", "mode.zen"};
        static const char* descs[kModeCount] = {"mode.classic.desc", "mode.timed.desc", "mode.target.desc",
                                                "mode.daily.desc", "mode.zen.desc"};
        const bool rtl = L.rtl();
        for (int i = 0; i < kModeCount; ++i) {
            const float y = 52.f + float(i) * 68.f, x = 22.f, w = 226.f, h = 62.f;
            if (ui.button("", x, y, w, h)) {
                c.mode = ModeId(i);
                c.go(SceneId::Game);
            }
            // Colour stripe with a droplet-coloured accent.
            g.rect(rtl ? x + w - 7 : x + 2, y + 3, 5, h - 6, t.blob[i * 2 + 1]);
            const float lx = rtl ? x + w - 16 : x + 16, rx = rtl ? x + 12 : x + w - 12;
            const Align la = ui.start(), ra = rtl ? Align::Left : Align::Right;
            g.text(L.tr(names[i]), lx, y + 16, 1, t.text, la);
            ui.paragraph(L.tr(descs[i]), rtl ? x + 12 : x + 16, y + 36, w - 28, 1, t.dim, la);
            const int best = i == int(ModeId::Daily) ? (c.save.dailyDay == platform::dayNumber() ? c.save.dailyToday : 0)
                                                      : c.save.best[i];
            if (best > 0) {
                const std::string b = std::to_string(best);
                g.number(b, rx, y + 16, 1, t.accent, ra);
                const float iw = float(g.numberW(b, 1));
                g.icon(IconId::Crown, rtl ? rx + iw + 4 : rx - iw - 13, y + 13, t.accent);
            }
        }
        if (ui.button(L.tr("common.back"), 60, 404, 150, 26) || c.in.esc) c.go(SceneId::Menu);
        langToggle(c, 242, 8);
    }
};

class ProgressScene final : public Scene {
public:
    void frame(Ctx& c, float) override {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        g.drawBackdrop(t, c.time);
        g.text(L.tr("progress.title"), cfg::kW / 2.f, 28, 2, t.accent, Align::Center, TextFx::Outline, t.ink);
        ui.panel(14, 46, 242, 352);

        const bool rtl = L.rtl();
        const float lx = rtl ? 244.f : 26.f, rx = rtl ? 26.f : 244.f;
        const Align la = ui.start(), ra = rtl ? Align::Left : Align::Right;
        static const char* names[kModeCount] = {"mode.classic", "mode.timed", "mode.target", "mode.daily", "mode.zen"};

        g.text(L.tr("progress.bests"), lx, 64, 1, t.accent, la);
        g.rect(22, 73, 226, 1, t.panelLo);
        for (int i = 0; i < kModeCount; ++i) {
            const float y = 88.f + float(i) * 18.f;
            g.text(L.tr(names[i]), lx, y, 1, t.text, la);
            g.number(std::to_string(c.save.best[i]), rx, y, 1, t.text, ra);
        }

        g.text(L.tr("progress.achievements"), lx, 190, 1, t.accent, la);
        g.rect(22, 199, 226, 1, t.panelLo);
        for (int i = 0; i < kAchCount; ++i) {
            const float y = 214.f + float(i) * 18.f;
            const bool got = c.save.has(i);
            const float ix = rtl ? 244.f - 7.f : 26.f;
            g.icon(got ? IconId::Check : IconId::Lock, ix, y - 3.f, got ? t.accent : t.dim);
            g.text(L.tr(achKey(i)), rtl ? 230.f : 42.f, y, 1, got ? t.text : t.dim, la);
        }
        g.text(L.fmt("menu.streak", std::to_string(c.save.streak)), cfg::kW / 2.f, 384.f, 1, t.dim, Align::Center);
        if (ui.button(L.tr("common.back"), 60, 410, 150, 26) || c.in.esc) c.go(SceneId::Menu);
    }
};

class SettingsScene final : public Scene {
public:
    void frame(Ctx& c, float) override {
        c.gfx.drawBackdrop(*c.theme, c.time);
        if (drawSettings(c, 72) || c.in.esc) c.go(SceneId::Menu);
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
        case SceneId::Brand: return makeBrandScene();
        default: return makeMenuScene();
    }
}

}  // namespace liq
