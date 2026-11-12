// The four Settings pages. Each page is one function that lays out its rows on a SettingsForm.
#include <cmath>
#include <string>
#include <vector>

#include "core/Access.hpp"
#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "game/Liquid.hpp"
#include "game/World.hpp"
#include "scenes/settings/SettingsForm.hpp"
#include "ui/widgets/Widgets.hpp"

namespace liq::settings {

namespace {

// A small pot on the Liquid page: it shows the chosen liquid sloshing, so the types can be compared by eye.
struct LiquidPreview {
    World w;
    int type = -1, quality = -1, frame = 0, n = 0;
    void reset(int t, int q) {
        type = t;
        quality = q;
        frame = n = 0;
        w.clear();
        w.fieldL = 0.f;
        w.fieldR = 120.f;
        w.floorY = 62.f;
        w.worldW = 130.f;
        w.worldH = 90.f;
        w.substeps = q + 1;
        applyLiquid(w, t);
    }
    void draw(Ctx& c, float x, float y) {
        if (type != c.save.liquid || quality != c.save.quality) reset(c.save.liquid, c.save.quality);
        if (frame % 90 == 0) {  // a drop every 1.5 s; the pot is emptied now and then
            if (n >= 6) {
                w.clear();
                n = 0;
            }
            static const int lv[6] = {1, 0, 2, 0, 1, 2};
            w.spawn(30.f + float((n * 37) % 60), 4.f, lv[n % 6]);
            ++n;
        }
        ++frame;
        w.step(cfg::kStep, cfg::kGravity);
        w.events.clear();
        w.impacts.clear();
        const Theme& t = *c.theme;
        c.gfx.rect(x - 1, y - 1, 122, 70, t.ink);
        c.gfx.rect(x, y, 120, 62, mix(t.tank, t.ink, 0.35f));
        std::vector<BlobSprite> pile;
        w.particleSprites(1.f, 0.f, pile);
        for (auto& sp : pile) {
            sp.x += x;
            sp.y += y;
        }
        c.raster.clear();
        c.raster.drawLiquid(pile, t, 9, 6, 1.1f);
        c.gfx.raster(c.raster);
        c.gfx.rect(x - 1, y + 62, 122, 3, t.frame);
    }
};

}  // namespace

// ---- page 0: general --------------------------------------------------------------------------
void generalPage(SettingsForm& f, bool allowCustom) {
    Ctx& c = f.c;
    Loc& L = c.loc;
    Ui& ui = c.ui;

    // Language: next language whose font is available.
    if (const int d = f.stepper("settings.language", langInfo(L.code()) ? langInfo(L.code())->name : "ENGLISH", true)) {
        int idx = 0;
        for (int i = 0; i < kLangCount; ++i)
            if (L.code() == langAt(i).code) idx = i;
        for (int k = 1; k <= kLangCount; ++k) {
            const LangInfo& li = langAt(((idx + d * k) % kLangCount + kLangCount) % kLangCount);
            if (c.setLanguage(li.code)) {
                c.save.store();
                break;
            }
        }
    }

    // Music and sound effects volume.
    if (f.percent("settings.music", c.save.musicVol)) {
        c.applySettings();
        c.save.store();
    }
    if (f.percent("settings.sfx", c.save.sfxVol)) {
        c.applySettings();
        c.save.store();
    }

    // Frame rate.
    {
        const int lim = c.save.fpsLimit();
        const bool numeric = lim > 0;
        const std::string v = numeric ? std::to_string(lim) : L.tr(lim == 0 ? "settings.vsync" : "settings.unlimited");
        if (const int d = f.stepper("settings.fps", v, numeric)) {
            c.save.fpsIdx = (c.save.fpsIdx + d + Save::kFpsChoiceCount) % Save::kFpsChoiceCount;
            c.applySettings();
            c.save.store();
        }
    }

    f.toggle("settings.showfps", c.save.showFps);
    if (!platform::isMobile() && f.toggle("settings.fullscreen", c.save.fullscreen)) c.applySettings();

    // Theme (unlock by playing).
    if (const int d = f.stepper("settings.theme", c.themeName(c.save.theme))) {
        c.cycleTheme(d);
        c.save.store();
    }

    if (allowCustom) {  // custom theme editor
        if (ui.button(L.tr("ct.title"), 26, f.y, 218, 20)) c.go(SceneId::ThemeEditor);
        f.skip(kRowH - 4.f);
    }
    f.skip(8.f);
    if (!c.gfx.hasPersianFont() && !L.rtl()) c.gfx.text(L.tr("settings.nofont"), 135, f.y + 4.f, 1, c.theme->dim, Align::Center);
}

// ---- page 1: game feel ------------------------------------------------------------------------
void feelPage(SettingsForm& f) {
    Save& s = f.c.save;
    f.toggle("settings.guide", s.guide);
    if (platform::isMobile()) f.toggle("settings.haptics", s.haptics);
    f.toggle("settings.shake", s.shake);
    f.toggle("settings.slowmo", s.slowmo);
    f.toggle("settings.calm", s.calm);
    f.toggle("settings.reduce", s.reduceMotion);
    if (platform::isMobile()) f.toggle("settings.lefthand", s.leftHand);  // phone-only layout option
}

// ---- page 2: accessibility --------------------------------------------------------------------
void accessPage(SettingsForm& f) {
    Ctx& c = f.c;
    if (const int d = f.stepper("settings.cvd", c.loc.tr(colorVisionKey(c.save.colorVision)))) {
        c.save.colorVision = (c.save.colorVision + d + kColorVisionCount) % kColorVisionCount;
        c.refreshTheme();
        c.save.store();
    }
    if (f.toggle("settings.contrast", c.save.highContrast)) c.refreshTheme();
    if (platform::isMobile()) f.toggle("settings.bigtouch", c.save.bigTouch);  // phone-only
    widgets::swatches(c.gfx, *c.theme, 26.f, f.y + 6.f, 12);  // live preview of the droplet palette
}

// ---- page 3: liquid ---------------------------------------------------------------------------
void liquidPage(SettingsForm& f) {
    Ctx& c = f.c;
    Loc& L = c.loc;
    Save& s = c.save;
    if (const int d = f.stepper("settings.quality", L.tr(s.quality == 0 ? "settings.q.low" : s.quality == 1 ? "settings.q.med" : "settings.q.high"))) {
        s.quality = (s.quality + d + 3) % 3;
        s.store();
    }
    if (const int d = f.stepper("liquid.type", L.tr(liquidProps(s.liquid).key))) {
        s.liquid = (s.liquid + d + kLiquidCount) % kLiquidCount;
        s.store();
    }
    f.skip(-4.f);
    const LiquidProps& lp = liquidProps(s.liquid);
    f.note(L.tr((std::string(lp.key) + ".desc").c_str()), 42.f);
    // How the type behaves compared with water (bars: 0..max).
    f.meterRow("liquid.thickness", std::log2(1.f + lp.thickness) / std::log2(7.f));
    f.meterRow("liquid.sticky", (lp.cohesion * 0.25f + lp.adhesion * 1.5f + lp.shear * 4.f + lp.jelly * 0.0004f) / 1.7f);
    f.meterRow("liquid.slip", lp.wallDrag);
    f.skip(6.f);
    static LiquidPreview preview;
    preview.draw(c, c.ui.mx(75.f, 120.f), f.y);
    f.skip(70.f);
    f.note(L.tr("liquid.note"), 0.f);
}

}  // namespace liq::settings
