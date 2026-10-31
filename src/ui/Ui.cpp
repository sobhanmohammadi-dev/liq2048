#include "ui/Ui.hpp"

#include "core/Config.hpp"
#include "scenes/Ctx.hpp"

namespace liq {

void Ui::beginFrame() { nextId_ = fidx_ = 0; }

void Ui::endFrame() {
    int n = fidx_;
    fcount_ = n;
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
    c_->gfx.rect(x, y, w, h, t.panel);
    c_->gfx.border(x, y, w, h, 2, t.dim);
}

bool Ui::button(const std::string& label, float x, float y, float w, float h, bool enabled, bool focusable) {
    Input& in = c_->in;
    const Theme& t = *c_->theme;
    Gfx& g = c_->gfx;
    int id = nextId_++;
    bool hover = in.mx >= x && in.mx < x + w && in.my >= y && in.my < y + h;
    bool kb = false;
    if (focusable && enabled) kb = (fidx_++ == focus_);
    bool click = false;
    if (enabled && in.pressed && hover) active_ = id;
    bool held = enabled && active_ == id && in.down && hover;
    if (enabled && in.released && hover && active_ == id) click = true;
    if (enabled && kb && in.confirm) click = true;

    Color fill = !enabled ? mix(t.panel, t.bg, 0.5f) : held ? mix(t.panel, t.accent, 0.45f)
                 : (hover || kb) ? mix(t.panel, t.text, 0.12f) : t.panel;
    g.rect(x, y, w, h, fill);
    g.border(x, y, w, h, 2, !enabled ? t.dim : (kb ? t.text : t.accent));
    if (!label.empty()) g.text(label, x + w / 2, y + h / 2 + (held ? 1 : 0), 2, enabled ? t.text : t.dim, Align::Center);
    if (click) c_->audio.play(Sfx::Click);
    return click;
}

void Ui::paragraph(const std::string& s, float x, float y, float w, int scale, Color col, Align a) {
    float line = 10.f * scale + 4;
    float ax = a == Align::Left ? x : (a == Align::Center ? x + w / 2 : x + w);
    for (const auto& l : c_->gfx.wrap(s, w, scale)) {
        c_->gfx.text(l, ax, y, scale, col, a);
        y += line;
    }
}

}  // namespace liq
