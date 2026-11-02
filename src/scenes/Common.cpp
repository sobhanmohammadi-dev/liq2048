// Widgets and drawing helpers shared by several scenes.
#include <cmath>
#include <string>

#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "game/World.hpp"
#include "scenes/Scenes.hpp"

namespace liq {

BlobSprite plainSprite(float x, float y, int level, float wobble, float phase) {
    BlobSprite s;
    s.x = x;
    s.y = y;
    s.level = level;
    const float r = World::radiusFor(level);
    if (wobble > 0.f) {
        const float th = phase;  // orientation of the stretch axis
        s.cs = std::cos(th);
        s.sn = std::sin(th);
        s.ra = r * (1.f + wobble);
        s.rb = r / (1.f + wobble);
    } else {
        s.ra = s.rb = r;
    }
    return s;
}

void drawWordmark(Ctx& c, float cx, float cy, int scale) {
    Gfx& g = c.gfx;
    const Theme& t = *c.theme;
    const std::string w = "LIQ2048";
    // Chunky extrusion: stack darker copies below, outline the lowest and the top copy.
    g.number(w, cx, cy + float(scale), scale, t.accentDark, Align::Center, TextFx::Outline, t.ink);
    for (int i = scale - 1; i >= 1; --i) g.number(w, cx, cy + float(i), scale, t.accentDark, Align::Center);
    g.number(w, cx, cy, scale, t.accent, Align::Center, TextFx::Outline, t.ink);
}

void drawTankFrame(Ctx& c) {
    Gfx& g = c.gfx;
    const Theme& t = *c.theme;
    const int L = int(cfg::kFieldL), R = int(cfg::kFieldR), top = cfg::kTankTop, fl = int(cfg::kFloor), bot = fl + 10;

    // Floor: ink edge, light top row, brick joints, dark bottom row.
    const int fx = L - 6, fw = R - L + 12;
    g.rect(float(fx), float(fl), float(fw), 10.f, t.frame);
    g.rect(float(fx), float(fl), float(fw), 1.f, t.ink);
    g.rect(float(fx + 1), float(fl + 1), float(fw - 2), 1.f, t.frameHi);
    g.rect(float(fx + 1), float(fl + 5), float(fw - 2), 1.f, t.frameLo);
    g.rect(float(fx + 1), float(fl + 8), float(fw - 2), 1.f, t.frameLo);
    g.rect(float(fx), float(fl + 9), float(fw), 1.f, t.ink);
    for (int x = fx + 10; x < fx + fw - 2; x += 24) g.rect(float(x), float(fl + 2), 1.f, 3.f, t.frameLo);
    for (int x = fx + 22; x < fx + fw - 2; x += 24) g.rect(float(x), float(fl + 6), 1.f, 2.f, t.frameLo);

    // Walls: ink | light | frame frame | dark | ink.
    auto wall = [&](int x) {
        g.rect(float(x), float(top), 1.f, float(bot - top), t.ink);
        g.rect(float(x + 1), float(top), 1.f, float(bot - top - 1), t.frameHi);
        g.rect(float(x + 2), float(top), 2.f, float(bot - top - 1), t.frame);
        g.rect(float(x + 4), float(top), 1.f, float(bot - top - 1), t.frameLo);
        g.rect(float(x + 5), float(top), 1.f, float(bot - top), t.ink);
    };
    wall(L - 6);
    wall(R);
    for (int y = top + 16; y < fl - 6; y += 34) {  // rivets
        g.rect(float(L - 4), float(y), 2.f, 2.f, t.frameLo);
        g.rect(float(R + 2), float(y), 2.f, 2.f, t.frameLo);
    }
    for (int x : {L - 8, R - 2}) {  // wall caps
        g.rect(float(x - 1), float(top - 5), 12.f, 6.f, t.ink);
        g.rect(float(x), float(top - 4), 10.f, 4.f, t.frame);
        g.rect(float(x), float(top - 4), 10.f, 1.f, t.frameHi);
    }
}

void drawDeadLine(Ctx& c, bool warn) {
    Gfx& g = c.gfx;
    const Theme& t = *c.theme;
    const int y = int(cfg::kDeadY);
    const int shift = warn ? int(c.time * 24.f) % 8 : 0;  // marching dashes while in danger
    const Color col = warn ? ((int(c.time * 8.f) & 1) ? t.danger : mix(t.danger, t.ink, 0.35f)) : mix(t.dim, t.tank, 0.35f);
    for (int x = int(cfg::kFieldL) - shift; x < int(cfg::kFieldR); x += 8) {
        const int x0 = std::max(x, int(cfg::kFieldL)), x1 = std::min(x + 5, int(cfg::kFieldR));
        if (x1 > x0) g.rect(float(x0), float(y), float(x1 - x0), 1.f, col);
    }
}

void langToggle(Ctx& c, float x, float y) {
    const Theme& t = *c.theme;
    const bool fa = c.loc.code() == "fa";
    const bool can = fa || c.gfx.hasPersianFont();
    const float w = 24, h = 14;
    const float bx = c.ui.mx(x, w);
    if (c.ui.button("", bx, y, w, h, can, false)) {
        c.setLanguage(fa ? "en" : "fa");
        c.save.store();
    }
    c.gfx.number(fa ? "EN" : "FA", bx + w / 2.f, y + h / 2.f, 1, can ? t.text : t.dim, Align::Center);
}

bool drawSettings(Ctx& c, float y0) {
    static int page = 0;  // 0 = general, 1 = game feel
    Gfx& g = c.gfx;
    Ui& ui = c.ui;
    const Theme& t = *c.theme;
    Loc& L = c.loc;
    const bool desktop = !platform::isMobile();
    const int rows = 7;
    const float rowH = 32.f;
    const float panelH = 40.f + rows * rowH + 40.f;
    ui.panel(14, y0, 242, panelH);
    g.text(L.tr("settings.title"), 135, y0 + 20, 2, t.accent, Align::Center, TextFx::Outline, t.ink);

    const bool rtl = L.rtl();
    const float lx = rtl ? 244.f : 26.f;
    const Align la = rtl ? Align::Right : Align::Left;
    float y = y0 + 40.f;
    auto label = [&](const char* key) { g.text(L.tr(key), lx, y + 10.f, 1, t.text, la); };
    const float cw = 112.f, cx = ui.mx(132.f, cw);  // control column

    auto toggle = [&](const char* key, bool& v) {
        label(key);
        if (ui.button(L.tr(v ? "settings.on" : "settings.off"), cx, y, cw, 20)) {
            v = !v;
            c.save.store();
        }
        y += rowH;
    };

    if (page == 1) {
        toggle("settings.guide", c.save.guide);
        if (platform::isMobile()) toggle("settings.haptics", c.save.haptics);
        toggle("settings.shake", c.save.shake);
        toggle("settings.slowmo", c.save.slowmo);
        toggle("settings.calm", c.save.calm);
        const float pw = 100.f;
        if (ui.button(L.tr("common.back"), 20, y0 + panelH - 34, pw, 24)) {
            page = 0;
            return true;
        }
        if (ui.button(L.tr("settings.general"), 150, y0 + panelH - 34, pw, 24)) page = 0;
        return false;
    }

    // Language
    label("settings.language");
    if (ui.stepper(cx, y, cw, 20, rtl ? "FA" : "EN", true) != 0) {
        if (c.setLanguage(rtl ? "en" : "fa")) c.save.store();
    }
    y += rowH;

    // Music and sound effects volume
    auto volumeRow = [&](const char* key, float& v) {
        label(key);
        if (const int d = ui.stepper(cx, y, cw, 20, std::to_string(int(std::lround(v * 100.f))) + "%", true)) {
            v = std::fmin(1.f, std::fmax(0.f, std::round((v + 0.1f * float(d)) * 10.f) / 10.f));
            c.applySettings();
            c.save.store();
        }
        y += rowH;
    };
    volumeRow("settings.music", c.save.musicVol);
    volumeRow("settings.sfx", c.save.sfxVol);

    // Frame rate
    label("settings.fps");
    {
        const int lim = c.save.fpsLimit();
        const bool numeric = lim > 0;
        const std::string v = numeric ? std::to_string(lim) : L.tr(lim == 0 ? "settings.vsync" : "settings.unlimited");
        if (const int d = ui.stepper(cx, y, cw, 20, v, numeric)) {
            c.save.fpsIdx = (c.save.fpsIdx + d + Save::kFpsChoiceCount) % Save::kFpsChoiceCount;
            c.applySettings();
            c.save.store();
        }
    }
    y += rowH;

    // FPS counter
    label("settings.showfps");
    if (ui.button(L.tr(c.save.showFps ? "settings.on" : "settings.off"), cx, y, cw, 20)) {
        c.save.showFps = !c.save.showFps;
        c.save.store();
    }
    y += rowH;

    // Fullscreen (desktop only)
    if (desktop) {
        label("settings.fullscreen");
        if (ui.button(L.tr(c.save.fullscreen ? "settings.on" : "settings.off"), cx, y, cw, 20)) {
            c.save.fullscreen = !c.save.fullscreen;
            c.applySettings();
            c.save.store();
        }
        y += rowH;
    }

    // Theme (unlock by playing)
    label("settings.theme");
    if (const int d = ui.stepper(cx, y, cw, 20, L.tr(themeAt(c.save.theme).key))) {
        c.cycleTheme(d);
        c.save.store();
    }
    y += rowH;

    if (!g.hasPersianFont() && !rtl) g.text(L.tr("settings.nofont"), 135, y - 4, 1, t.dim, Align::Center);

    if (ui.button(L.tr("common.back"), 20, y0 + panelH - 34, 100, 24)) return true;
    if (ui.button(L.tr("settings.more"), 150, y0 + panelH - 34, 100, 24)) page = 1;
    return false;
}

}  // namespace liq
