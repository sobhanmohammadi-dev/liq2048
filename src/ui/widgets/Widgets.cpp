#include "ui/widgets/Widgets.hpp"

#include <algorithm>
#include <cmath>

#include "core/Config.hpp"
#include "core/Util.hpp"
#include "gfx/Icons.hpp"

namespace liq::widgets {

void meter(Gfx& g, const Theme& t, float x, float y, float w, float h, float frac) {
    g.rect(x, y, w, h, t.ink);
    g.rect(x + 1, y + 1, w - 2, h - 2, t.panelLo);
    g.rect(x + 1, y + 1, (w - 2) * clampv(frac, 0.f, 1.f), h - 2, t.accent);
}

void segments(Gfx& g, const Theme& t, float y, int count, float frac) {
    const int on = int(std::lround(clampv(frac, 0.f, 1.f) * float(count)));
    const float x0 = (cfg::kW - float(count) * 6.f + 1.f) / 2.f;
    for (int i = 0; i < count; ++i) g.rect(x0 + float(i) * 6.f, y, 5, 4, i < on ? t.accent : t.panelLo);
}

void pageDots(Gfx& g, const Theme& t, float cx, float y, int count, int active) {
    const float x0 = cx - float(count) * 5.f + 3.f;  // 6 px squares on a 10 px pitch
    for (int i = 0; i < count; ++i) g.rect(x0 + float(i) * 10.f, y, 6, 6, i == active ? t.accent : t.panelLo);
}

void spinner(Gfx& g, const Theme& t, float cx, float y, float time) {
    const int lit = int(time * 3.f) % 4;
    for (int i = 0; i < 4; ++i) g.rect(cx - 15.f + float(i) * 10.f, y, 6, 6, i == lit ? t.accent : t.panelLo);
}

void swatches(Gfx& g, const Theme& t, float x, float y, int count, float cell, float pitch) {
    for (int i = 0; i < count; ++i) {
        const float px = x + float(i) * pitch;
        g.rect(px, y, cell, cell, t.ink);
        g.rect(px + 1.f, y + 1.f, cell - 2.f, cell - 2.f, t.blob[std::min(i, 11)]);
    }
}

void levelStrip(Gfx& g, const Theme& t, float y, int maxLevel) {
    for (int i = 0; i < 12; ++i) {
        const float x = cfg::kW / 2.f + (float(i) - 5.5f) * 19.f;
        if (i <= 3 || i <= maxLevel) {  // the first four are always shown
            g.disc(x, y, 4, t.ink);
            g.disc(x, y, 3, t.blob[i]);
        } else {
            g.ring(x, y, 3, t.panelLo);
        }
    }
    if (maxLevel > 3) g.icon(IconId::Marker, cfg::kW / 2.f + (float(maxLevel) - 5.5f) * 19.f - 2.f, y + 6.f, t.accent);
}

void banner(Gfx& g, const Theme& t, float y, float w, const std::string& text) {
    const auto lines = g.wrap(text, w - 16.f, 1);
    const float lh = float(g.lineH(1));
    const float h = 12.f + float(lines.size()) * lh;
    g.panel((cfg::kW - w) / 2.f, y, w, h, t.panel, t.panelHi, t.panelLo, t.accent);
    for (size_t i = 0; i < lines.size(); ++i) g.text(lines[i], cfg::kW / 2.f, y + 10.f + float(i) * lh, 1, t.text, Align::Center);
}

void toast(Gfx& g, const Theme& t, const std::string& text, float timeLeft, float total) {
    const float slide = std::min(1.f, std::min(timeLeft, total - timeLeft) * 5.f);
    const float y = std::floor(-26.f + (104.f + 26.f) * slide);
    const float w = std::max(120.f, float(g.textW(text, 1)) + 44.f);
    const float x = (cfg::kW - w) / 2.f;
    g.panel(x, y, w, 22, t.panel, t.panelHi, t.panelLo, t.accent);
    g.icon(IconId::Check, x + 8, y + 8, t.accent);
    g.text(text, x + 22, y + 11, 1, t.text, Align::Left);
}

float slideIn(float t) { return std::floor((1.f - std::min(1.f, t * 4.f)) * -60.f); }

void chip(Gfx& g, const Theme& t, float x, float y, float r, Color fill) {
    g.disc(x, y, int(r) + 1, t.ink);
    g.disc(x, y, int(r), fill);
}

void crownCaption(Gfx& g, const Theme& t, float cx, float y, const std::string& text) {
    const float w = float(g.textW(text, 1));
    g.text(text, cx, y + 3.f, 1, t.accent, Align::Center);
    g.icon(IconId::Crown, cx - w / 2.f - 14.f, y, t.accent);
    g.icon(IconId::Crown, cx + w / 2.f + 5.f, y, t.accent);
}

}  // namespace liq::widgets
