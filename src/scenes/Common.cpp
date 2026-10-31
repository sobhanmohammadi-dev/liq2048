// Widgets shared by several scenes: settings panel, language switcher, backdrop.
#include <cmath>
#include <string>

#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "scenes/Scenes.hpp"

namespace liq {

void drawStars(Ctx& c) {
    const Theme& t = *c.theme;
    float tm = SDL_GetTicks() / 1000.f;
    for (int i = 0; i < 40; ++i) {
        int sx = (i * 97 + 13) % cfg::kW, sy = (i * 181 + 29) % cfg::kH;
        bool tw = std::sin(tm * 2.f + float(i)) > 0.6f;
        c.gfx.rect(float(sx), float(sy), tw ? 2.f : 1.f, tw ? 2.f : 1.f, withAlpha(t.dim, tw ? 200 : 110));
    }
}

void langToggle(Ctx& c, float x, float y) {
    const bool fa = c.loc.code() == "fa";
    // Label is the language you would switch TO, drawn with the bitmap font.
    const std::string label = fa ? "EN" : "FA";
    bool can = fa || c.gfx.hasPersianFont();
    float w = 40, h = 22;
    float bx = c.ui.mx(x, w);
    const Theme& t = *c.theme;
    bool hover = c.in.mx >= bx && c.in.mx < bx + w && c.in.my >= y && c.in.my < y + h;
    c.gfx.rect(bx, y, w, h, hover && can ? mix(t.panel, t.text, 0.12f) : t.panel);
    c.gfx.border(bx, y, w, h, 2, can ? t.dim : mix(t.dim, t.bg, 0.6f));
    c.gfx.bitmap(label, bx + w / 2.f, y + h / 2.f, 2, can ? t.text : t.dim, Align::Center);
    if (can && hover && c.in.released) {
        c.audio.play(Sfx::Click);
        c.setLanguage(fa ? "en" : "fa");
        c.save.store();
    }
}

bool drawSettings(Ctx& c, float y0) {
    Gfx& g = c.gfx;
    Ui& ui = c.ui;
    const Theme& t = *c.theme;
    Loc& L = c.loc;
    g.text(L.tr("settings.title"), 180, y0, 3, t.accent, Align::Center);

    const float rowH = 54.f;
    float y = y0 + 40.f;
    const bool rtl = L.rtl();
    const float lx = rtl ? 330.f : 30.f;
    const Align la = rtl ? Align::Right : Align::Left;
    auto label = [&](const char* key, float yy) { g.text(L.tr(key), lx, yy + 20.f, 2, t.text, la); };

    // Language
    label("settings.language", y);
    {
        const bool fa = L.code() == "fa";
        if (ui.button(fa ? "FA" : "EN", ui.mx(200, 130), y, 130, 40)) {
            if (c.setLanguage(fa ? "en" : "fa")) c.save.store();
        }
    }
    y += rowH;

    // Volume
    label("settings.volume", y);
    if (ui.button("-", ui.mx(200, 34), y, 34, 40)) {
        c.save.volume = std::fmax(0.f, std::round((c.save.volume - 0.1f) * 10.f) / 10.f);
        c.applySettings();
        c.save.store();
    }
    g.bitmap(std::to_string(int(std::lround(c.save.volume * 100.f))), ui.mx(234, 62) + 31, y + 20, 2, t.text,
             Align::Center);
    if (ui.button("+", ui.mx(296, 34), y, 34, 40)) {
        c.save.volume = std::fmin(1.f, std::round((c.save.volume + 0.1f) * 10.f) / 10.f);
        c.applySettings();
        c.save.store();
    }
    y += rowH;

    // Fullscreen (desktop only)
    if (!platform::isMobile()) {
        label("settings.fullscreen", y);
        if (ui.button(L.tr(c.save.fullscreen ? "settings.on" : "settings.off"), ui.mx(200, 130), y, 130, 40)) {
            c.save.fullscreen = !c.save.fullscreen;
            c.applySettings();
            c.save.store();
        }
        y += rowH;
    }

    // Theme (unlock by playing)
    label("settings.theme", y);
    {
        bool next = c.save.theme;
        (void)next;
        if (ui.button(L.tr(themeAt(c.save.theme).key), ui.mx(170, 160), y, 160, 40)) {
            c.cycleTheme();
            c.save.store();
        }
    }
    y += rowH;

    if (!g.hasPersianFont() && L.code() != "fa")
        g.text(L.tr("settings.nofont"), 180, y - 4, 1, t.dim, Align::Center);

    return ui.button(L.tr("common.back"), 80, y + 12, 200, 40);
}

}  // namespace liq
