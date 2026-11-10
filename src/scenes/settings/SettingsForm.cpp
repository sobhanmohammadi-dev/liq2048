#include "scenes/settings/SettingsForm.hpp"

#include <algorithm>
#include <cmath>

#include "ui/widgets/Widgets.hpp"

namespace liq::settings {

SettingsForm::SettingsForm(Ctx& ctx, float firstRowY) : c(ctx), y(firstRowY) {
    // Labels sit opposite the controls in an RTL language or the left-hand layout.
    const bool flip = c.loc.rtl() != c.ui.mirrored();
    lx = flip ? 244.f : 26.f;
    la = flip ? Align::Right : Align::Left;
    cw = 112.f;
    cx = c.ui.mx(132.f, cw);
}

void SettingsForm::label(const char* key) const { c.gfx.text(c.loc.tr(key), lx, y + 10.f, 1, c.theme->text, la); }

bool SettingsForm::toggle(const char* key, bool& v) {
    label(key);
    const bool hit = c.ui.button(c.loc.tr(v ? "settings.on" : "settings.off"), cx, y, cw, 20);
    if (hit) {
        v = !v;
        c.save.store();
    }
    next();
    return hit;
}

int SettingsForm::stepper(const char* key, const std::string& value, bool bitmapValue) {
    label(key);
    const int d = c.ui.stepper(cx, y, cw, 20, value, bitmapValue);
    next();
    return d;
}

bool SettingsForm::percent(const char* key, float& v) {
    const int d = stepper(key, std::to_string(int(std::lround(v * 100.f))) + "%", true);
    if (!d) return false;
    v = std::fmin(1.f, std::fmax(0.f, std::round((v + 0.1f * float(d)) * 10.f) / 10.f));
    return true;
}

void SettingsForm::meterRow(const char* key, float frac) {
    c.gfx.text(c.loc.tr(key), lx, y + 3.f, 1, c.theme->text, la);
    const float bw = 100.f;
    widgets::meter(c.gfx, *c.theme, c.ui.mx(144.f, bw), y, bw, 6.f, frac);
    y += 12.f;
}

void SettingsForm::note(const std::string& text, float gapAfter) {
    c.ui.paragraph(text, 24, y + 2.f, 222, 1, c.theme->dim, Align::Center);
    y += gapAfter;
}

}  // namespace liq::settings
