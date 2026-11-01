#include "ui/Ui.hpp"

#include "core/Config.hpp"
#include "scenes/Ctx.hpp"

namespace liq {

void Ui::beginFrame() { nextId_ = fidx_ = 0; }

void Ui::endFrame() {
    const int n = fidx_;
    if (n > 0) {
        if (c_->in.dn) focus_ = (focus_ + 1) % n;
        if (c_->in.up) focus_ = (focus_ + n - 1) % n;
        if (focus_ >= n) focus_ = 0;
    }
    if (c_->in.released) active_ = -1;
}

float Ui::mx(float x, float w) const { return c_->loc.rtl() ? cfg::kW - x - w : x; }
Align Ui::start() const { return c_->loc.rtl() ? Align::Right : Align::Left; }

void Ui::panel(float x, float y, float w, float h) {
    const Theme& t = *c_->theme;
    c_->gfx.panel(x, y, w, h, t.panel, t.panelHi, t.panelLo, t.ink);
}

Ui::Hit Ui::hit(float x, float y, float w, float h, bool enabled, bool focusable) {
    Input& in = c_->in;
    Hit r{};
    const int id = nextId_++;
    r.hover = enabled && in.mx >= x && in.mx < x + w && in.my >= y && in.my < y + h;
    if (focusable && enabled) r.focus = (fidx_++ == focus_);
    if (enabled && in.pressed && r.hover) active_ = id;
    r.held = enabled && active_ == id && in.down && r.hover;
    if (enabled && in.released && r.hover && active_ == id) r.click = true;
    if (enabled && r.focus && in.confirm) r.click = true;
    return r;
}

bool Ui::button(const std::string& label, float x, float y, float w, float h, bool enabled, bool focusable,
                BtnStyle style) {
    const Theme& t = *c_->theme;
    Gfx& g = c_->gfx;
    const Hit s = hit(x, y, w, h, enabled, focusable);

    Color fill = style == BtnStyle::Primary ? t.accent : t.panel;
    Color hi = style == BtnStyle::Primary ? mix(t.accent, rgb(0xffffff), 0.45f) : t.panelHi;
    Color lo = style == BtnStyle::Primary ? t.accentDark : t.panelLo;
    Color txt = style == BtnStyle::Primary ? t.ink : t.text;
    if (!enabled) {
        fill = mix(t.panel, t.sky[2], 0.55f);
        hi = lo = fill;
        txt = t.dim;
    } else if (s.held) {
        std::swap(hi, lo);
        fill = mix(fill, t.ink, 0.18f);
    } else if (s.hover || s.focus) {
        fill = mix(fill, rgb(0xffffff), 0.10f);
    }
    g.panel(x, y, w, h, fill, hi, lo, s.focus ? t.accent : t.ink);
    if (s.focus) g.border(x - 1, y - 1, w + 2, h + 2, 1, withAlpha(t.accent, 255));
    const float ty = y + h / 2.f + (s.held ? 1.f : 0.f);
    if (!label.empty()) g.text(label, x + w / 2.f, ty, 1, txt, Align::Center);
    if (s.click) c_->audio.play(Sfx::Click);
    return s.click;
}

bool Ui::iconButton(IconId icon, float x, float y, float w, float h, bool enabled, bool focusable) {
    const Theme& t = *c_->theme;
    Gfx& g = c_->gfx;
    const Hit s = hit(x, y, w, h, enabled, focusable);
    Color fill = t.panel, hi = t.panelHi, lo = t.panelLo;
    if (!enabled) {
        fill = mix(t.panel, t.sky[2], 0.55f);
        hi = lo = fill;
    } else if (s.held) {
        std::swap(hi, lo);
        fill = mix(fill, t.ink, 0.18f);
    } else if (s.hover || s.focus) {
        fill = mix(fill, rgb(0xffffff), 0.10f);
    }
    g.panel(x, y, w, h, fill, hi, lo, s.focus ? t.accent : t.ink);
    const IconDef& d = iconDef(icon);
    g.icon(icon, x + float((int(w) - d.w) / 2), y + float((int(h) - d.h) / 2) + (s.held ? 1.f : 0.f),
           enabled ? t.text : t.dim);
    if (s.click) c_->audio.play(Sfx::Click);
    return s.click;
}

int Ui::stepper(float x, float y, float w, float h, const std::string& value, bool bitmapValue) {
    const Theme& t = *c_->theme;
    int dir = 0;
    const float bw = h;  // square arrow buttons
    if (iconButton(IconId::Left, x, y, bw, h, true, true)) dir = -1;
    if (iconButton(IconId::Right, x + w - bw, y, bw, h, true, true)) dir = +1;
    if (bitmapValue) c_->gfx.number(value, x + w / 2.f, y + h / 2.f, 1, t.text, Align::Center);
    else c_->gfx.text(value, x + w / 2.f, y + h / 2.f, 1, t.text, Align::Center);
    return dir;
}

void Ui::paragraph(const std::string& s, float x, float y, float w, int scale, Color col, Align a) {
    const float line = float(c_->gfx.lineH(scale));
    const float ax = a == Align::Left ? x : (a == Align::Center ? x + w / 2 : x + w);
    for (const auto& l : c_->gfx.wrap(s, w, scale)) {
        c_->gfx.text(l, ax, y, scale, col, a);
        y += line;
    }
}

}  // namespace liq
