// Widgets and drawing helpers shared by several scenes.
#include <cctype>
#include <cmath>
#include <string>

#include "core/Access.hpp"
#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "game/Liquid.hpp"
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

void drawDeadLine(Ctx& c, bool warn, float lineY) {
    Gfx& g = c.gfx;
    const Theme& t = *c.theme;
    const int y = int(lineY);
    const int shift = warn ? int(c.time * 24.f) % 8 : 0;  // marching dashes while in danger
    const Color col = warn ? ((int(c.time * 8.f) & 1) ? t.danger : mix(t.danger, t.ink, 0.35f)) : mix(t.dim, t.tank, 0.35f);
    for (int x = int(cfg::kFieldL) - shift; x < int(cfg::kFieldR); x += 8) {
        const int x0 = std::max(x, int(cfg::kFieldL)), x1 = std::min(x + 5, int(cfg::kFieldR));
        if (x1 > x0) g.rect(float(x0), float(y), float(x1 - x0), 1.f, col);
    }
}

static int g_page = 0;  // 0 = general, 1 = game feel, 2 = accessibility, 3 = liquid
void setSettingsPage(int p) { g_page = p < 0 || p > 3 ? 0 : p; }

// A small pot on the Liquid settings page: it shows the chosen liquid sloshing, so the types can be compared by eye.
namespace {
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

bool drawSettings(Ctx& c, float y0, bool allowCustom) {
    int& page = g_page;
    Gfx& g = c.gfx;
    Ui& ui = c.ui;
    const Theme& t = *c.theme;
    Loc& L = c.loc;
    const bool desktop = !platform::isMobile();
    const int rows = g_page == 0 ? 9 : 8;
    const float rowH = 32.f;
    const float panelH = 40.f + rows * rowH + 40.f;
    ui.panel(14, y0, 242, panelH);
    g.text(L.tr("settings.title"), 135, y0 + 20, 2, t.accent, Align::Center, TextFx::Outline, t.ink);

    const bool rtl = L.rtl();
    const bool flip = rtl != ui.mirrored();  // labels sit opposite the controls (RTL language or left-hand layout)
    const float lx = flip ? 244.f : 26.f;
    const Align la = flip ? Align::Right : Align::Left;
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
        toggle("settings.reduce", c.save.reduceMotion);
        toggle("settings.lefthand", c.save.leftHand);
    } else if (page == 3) {
        label("settings.quality");
        if (const int d = ui.stepper(cx, y, cw, 20, L.tr(c.save.quality == 0 ? "settings.q.low" : c.save.quality == 1 ? "settings.q.med" : "settings.q.high"))) {
            c.save.quality = (c.save.quality + d + 3) % 3;
            c.save.store();
        }
        y += rowH;
        label("liquid.type");
        if (const int d = ui.stepper(cx, y, cw, 20, L.tr(liquidProps(c.save.liquid).key))) {
            c.save.liquid = (c.save.liquid + d + kLiquidCount) % kLiquidCount;
            c.save.store();
        }
        y += rowH - 4.f;
        const LiquidProps& lp = liquidProps(c.save.liquid);
        ui.paragraph(L.tr((std::string(lp.key) + ".desc").c_str()), 24, y + 2.f, 222, 1, t.dim, Align::Center);
        y += 42.f;
        // How the type behaves compared with water (bars: 0..max)
        auto bar = [&](const char* key, float v, float vmax) {
            g.text(L.tr(key), lx, y + 3.f, 1, t.text, la);
            const float bw = 100.f, bx = ui.mx(144.f, bw);
            g.rect(bx, y, bw, 6, t.ink);
            g.rect(bx + 1, y + 1, bw - 2, 4, t.panelLo);
            g.rect(bx + 1, y + 1, (bw - 2) * std::fmin(1.f, v / vmax), 4, t.accent);
            y += 12.f;
        };
        bar("liquid.thickness", std::log2(1.f + lp.thickness) , std::log2(7.f));
        bar("liquid.sticky", lp.cohesion * 0.25f + lp.adhesion * 1.5f + lp.shear * 4.f + lp.jelly * 0.0004f, 1.7f);
        bar("liquid.slip", lp.wallDrag, 1.f);
        y += 6.f;
        static LiquidPreview pv;
        pv.draw(c, ui.mx(75.f, 120.f), y);
        y += 70.f;
        ui.paragraph(L.tr("liquid.note"), 24, y + 2.f, 222, 1, t.dim, Align::Center);
    } else if (page == 2) {
        label("settings.cvd");
        if (const int d = ui.stepper(cx, y, cw, 20, L.tr(colorVisionKey(c.save.colorVision)))) {
            c.save.colorVision = (c.save.colorVision + d + kColorVisionCount) % kColorVisionCount;
            c.refreshTheme();
            c.save.store();
        }
        y += rowH;
        label("settings.contrast");
        if (ui.button(L.tr(c.save.highContrast ? "settings.on" : "settings.off"), cx, y, cw, 20)) {
            c.save.highContrast = !c.save.highContrast;
            c.refreshTheme();
            c.save.store();
        }
        y += rowH;
        toggle("settings.bigtouch", c.save.bigTouch);
        // Live preview of the droplet palette.
        for (int i = 0; i < 12; ++i) {
            const float px = 26.f + float(i) * 18.f;
            g.rect(px, y + 6.f, 16, 16, t.ink);
            g.rect(px + 1.f, y + 7.f, 14, 14, t.blob[i]);
        }
    }
    if (page != 0) {
        if (ui.button(L.tr("common.back"), 20, y0 + panelH - 34, 100, 24)) {
            page = 0;
            return true;
        }
        if (ui.button(L.tr(page == 1 ? "settings.liquid" : page == 3 ? "settings.access" : "settings.general"), 150, y0 + panelH - 34, 100, 24)) page = page == 1 ? 3 : page == 3 ? 2 : 0;
        return false;
    }

    // Language
    label("settings.language");
    if (const int d = ui.stepper(cx, y, cw, 20, langInfo(L.code()) ? langInfo(L.code())->name : "ENGLISH", true)) {
        int idx = 0;
        for (int i = 0; i < kLangCount; ++i)
            if (L.code() == langAt(i).code) idx = i;
        for (int k = 1; k <= kLangCount; ++k) {  // next language whose font is available
            const LangInfo& li = langAt(((idx + d * k) % kLangCount + kLangCount) % kLangCount);
            if (c.setLanguage(li.code)) {
                c.save.store();
                break;
            }
        }
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
    if (const int d = ui.stepper(cx, y, cw, 20, c.themeName(c.save.theme))) {
        c.cycleTheme(d);
        c.save.store();
    }
    y += rowH;

    // Custom theme editor
    if (allowCustom) {
        if (ui.button(L.tr("ct.title"), 26, y, 218, 20)) c.go(SceneId::ThemeEditor);
        y += rowH - 4.f;
    }
    // Liquid type and quality
    if (ui.button(L.tr("settings.liquid"), 26, y, 218, 20)) page = 3;
    y += 22.f;
    if (!g.hasPersianFont() && !rtl) g.text(L.tr("settings.nofont"), 135, y + 4.f, 1, t.dim, Align::Center);

    if (ui.button(L.tr("common.back"), 20, y0 + panelH - 34, 100, 24)) return true;
    if (ui.button(L.tr("settings.more"), 150, y0 + panelH - 34, 100, 24)) page = 1;
    return false;
}

}  // namespace liq
