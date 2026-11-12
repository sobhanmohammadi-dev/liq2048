// Small looping pictures for the mode tiles: one hand-built animation per mode (see ModeIcons.hpp).
#include "scenes/modes/ModeIcons.hpp"

#include <algorithm>
#include <cmath>

#include "core/Util.hpp"
#include "game/World.hpp"

namespace liq {

namespace {

void line(Gfx& g, float x0, float y0, float x1, float y1, Color col) {
    const int n = std::max(1, int(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0))));
    for (int i = 0; i <= n; ++i) {
        const float u = float(i) / float(n);
        g.rect(std::floor(x0 + (x1 - x0) * u), std::floor(y0 + (y1 - y0) * u), 1, 1, col);
    }
}

// ---- Animated pictures ----------------------------------------------------------------------------------------------
// Every picture is a pure function of a looping phase built from sin/cos/smoothstep and grows or fades in and out from
// nothing, so when the loop wraps the first frame equals the last one: it never looks like "finished, start again".
constexpr float kTau = 6.2831853f;
float sstep(float e0, float e1, float x) {
    const float u = clampv((x - e0) / (e1 - e0), 0.f, 1.f);
    return u * u * (3.f - 2.f * u);
}
float pulse(float p) { return 0.5f - 0.5f * std::cos(p * kTau); }  // 0 at p = 0 and 1, 1 at p = 0.5
// Sprite whose size follows `scale` (0 = not there); the field draw fuses neighbours like in the game.
void put(std::vector<BlobSprite>& sp, float x, float y, int lvl, float scale, float wob, float ph) {
    if (scale < 0.04f) return;
    BlobSprite b = plainSprite(x, y, lvl, wob, ph);
    b.ra *= scale;
    b.rb *= scale;
    sp.push_back(b);
}

}  // namespace

void drawModeIcon(Ctx& c, ModeId m, float cx, float cy, std::vector<BlobSprite>& sp) {
    Gfx& g = c.gfx;
    const Theme& t = *c.theme;
    const float T = c.save.reduceMotion ? 0.f : c.time;
    const float wob = 0.06f + 0.03f * std::sin(T * 2.f);
    switch (m) {
        case ModeId::Battle: {  // two drops shoving each other, sparks on every clash
            const float push = std::sin(T * 2.4f);
            const float r1 = World::radiusFor(1), r2 = World::radiusFor(2);
            const float gap = 24.f + 12.f * (1.f - push) * 0.5f;
            const float total = r1 + r2 + gap;
            const float x1 = cx - total / 2.f + r1 + push * 3.f, x2 = cx + total / 2.f - r2 + push * 3.f;
            const float hit = sstep(0.55f, 1.f, push);
            sp.push_back(plainSprite(x1, cy + 4.f, 1, wob + 0.06f * hit, T));
            sp.push_back(plainSprite(x2, cy + 4.f, 2, wob + 0.06f * hit, T + 1.f));
            if (hit > 0.02f)
                for (int i = 0; i < 4; ++i) {
                    const float a = float(i) * 1.5708f + 0.785f, rr = 6.f + 5.f * hit;
                    g.rect(cx + 2.f + std::cos(a) * rr, cy - 14.f + std::sin(a) * rr, 2, 2, withAlpha(t.accent, int(255.f * hit)));
                }
            break;
        }
        case ModeId::Puzzle: {  // three drops (2, 4, 16) wandering
            const int lv[3] = {1, 2, 4};
            for (int i = 0; i < 3; ++i) {
                const float ph = float(i) * 2.1f;
                put(sp, cx + std::sin(T * 0.9f + ph) * 12.f, cy - 36.f + float(i) * 36.f + std::sin(T * 1.4f + ph * 1.3f) * 6.f, lv[i], 0.72f, wob, T * 0.5f + ph);
            }
            break;
        }
        case ModeId::Zen: {  // one calm drop, ripples that fade in and out
            for (int i = 0; i < 3; ++i) {
                const float ph = std::fmod(T * 0.2f + float(i) / 3.f, 1.f);
                g.ring(cx, cy + 2.f, 12 + int(ph * 30.f), withAlpha(t.blob[0], int(120.f * std::sin(ph * 3.1415927f))));
            }
            put(sp, cx, cy + 2.f + std::sin(T * 0.8f) * 2.f, 0, 1.f, 0.04f + 0.03f * std::sin(T * 0.8f), T * 0.3f);
            break;
        }
        case ModeId::Classic: {  // two drops glide together, fuse, and ease apart again
            const float k = pulse(std::fmod(T / 3.6f, 1.f));  // 0 apart .. 1 fused
            const float d = 2.f + 17.f * (1.f - k) * (1.f - k);
            const float jig = 0.16f * sstep(0.7f, 1.f, k) * std::cos(T * 9.f);
            put(sp, cx - d, cy + 3.f, 0, 1.f, wob + jig, T);
            put(sp, cx + d, cy + 3.f, 0, 1.f, wob + jig, T + 2.f);
            break;
        }
        case ModeId::Timed: {  // a clock; the hour hand and the sweeping hand never stop
            const Color face = mix(t.panelHi, t.text, 0.1f);
            g.disc(cx, cy, 17, t.ink);
            g.disc(cx, cy, 15, face);
            for (int i = 0; i < 12; ++i) {
                const float a = float(i) * 0.5236f;
                g.rect(std::floor(cx + std::sin(a) * 12.f), std::floor(cy - std::cos(a) * 12.f), 1, 1, t.ink);
            }
            const float a = T * 1.6f, b = T * 0.13f;
            line(g, cx, cy, cx + std::sin(a) * 12.f, cy - std::cos(a) * 12.f, t.danger);
            line(g, cx, cy, cx + std::sin(b) * 8.f, cy - std::cos(b) * 8.f, t.ink);
            g.rect(cx - 1, cy - 1, 2, 2, t.ink);
            g.rect(cx - 2, cy - 21, 4, 3, t.ink);
            break;
        }
        case ModeId::Target: {  // a drop grows at the top, falls in, splashes and melts into the bullseye
            const float p = std::fmod(T / 2.6f, 1.f);
            g.ring(cx, cy + 6.f, 17, t.text);
            g.ring(cx, cy + 6.f, 11, t.dim);
            g.ring(cx, cy + 6.f, 5 + int(pulse(std::fmod(T / 1.3f, 1.f)) * 2.f), t.accent);
            const float fall = sstep(0.18f, 0.55f, p);
            const float y = cy - 15.f + fall * fall * 22.f;
            const float scale = sstep(0.f, 0.18f, p) * (1.f - sstep(0.78f, 1.f, p));
            const float sq = 0.14f * sstep(0.55f, 0.6f, p) * std::exp(-(p - 0.55f) * 9.f) * std::cos((p - 0.55f) * 40.f);
            put(sp, cx, y, 0, scale * 0.8f, sq + 0.02f, T);
            break;
        }
        case ModeId::Daily:
        case ModeId::Weekly: {  // calendar page
            const bool week = m == ModeId::Weekly;
            const float w = 34.f, h = 32.f, x = cx - w / 2.f, y = cy - h / 2.f + 2.f;
            g.rect(x, y, w, h, mix(t.panelHi, t.text, 0.12f));
            g.border(x, y, w, h, 1, t.ink);
            g.rect(x + 1, y + 1, w - 2, 8, t.danger);
            g.rect(x + 7, y - 3, 2, 6, t.ink);
            g.rect(x + w - 9, y - 3, 2, 6, t.ink);
            const int on = week ? int(T * 1.6f) % 7 : 5;
            int n = 0;
            for (int r = 0; r < 3; ++r)
                for (int q = 0; q < 4; ++q, ++n) {
                    if (week && n >= 7) break;
                    const bool hi = n == on;
                    g.rect(x + 4.f + float(q) * 7.f, y + 13.f + float(r) * 6.f, hi ? 5 : 4, hi ? 5 : 4, hi ? t.accent : mix(t.dim, t.ink, 0.3f));
                }
            if (!week) {  // a sparkle that swells and fades on the day of the challenge
                const float s = pulse(std::fmod(T / 1.8f, 1.f)), sx = x + 13.f, sy = y + 9.f;
                const int a = int(255.f * s), l = 1 + int(s * 3.f);
                g.rect(sx - l, sy, 2 * l + 1, 1, withAlpha(t.accent, a));
                g.rect(sx, sy - l, 1, 2 * l + 1, withAlpha(t.accent, a));
            }
            break;
        }
        case ModeId::BigBox: {  // a wide container, four players' drops and a camera frame panning along
            const float w = 58.f, h = 32.f, x0 = cx - w / 2.f, top = cy - h / 2.f;
            g.rect(x0, top, w, h, withAlpha(t.ink, 110));
            const Color wc = mix(t.blob[6], t.sky[1], 0.4f);
            for (int x = 0; x < int(w); ++x) {
                const float wy = top + h - 9.f + std::sin(T * 2.f + float(x) * 0.3f) * 1.2f;
                g.rect(x0 + float(x), wy, 1, top + h - wy, wc);
            }
            g.border(x0 - 1, top - 1, w + 2, h + 2, 1, t.ink);
            for (int i = 0; i < 4; ++i) {  // players' drops bobbing in the water
                const float px = x0 + 8.f + float(i) * 14.f;
                put(sp, px, top + h - 11.f + std::sin(T * 1.7f + float(i) * 1.6f) * 2.f, i, 0.45f, 0.05f, T + float(i));
            }
            const float fx = x0 + (w - 26.f) * (0.5f + 0.5f * std::sin(T * 1.1f));  // camera frame
            g.border(fx, top + 2, 26, h - 4, 1, t.accent);
            g.rect(fx, top + 2, 3, 1, t.text);
            g.rect(fx + 23, top + h - 3, 3, 1, t.text);
            break;
        }
        case ModeId::Madhouse: {  // drops that keep changing number, size and colour (every swap hides at size 0)
            const int seq[6] = {0, 3, 1, 5, 2, 4};
            for (int i = 0; i < 3; ++i) {
                const float phi = T * 0.9f + float(i) * 1.05f;
                const float sa = std::cos(phi) * std::cos(phi), sb = 1.f - sa;
                const int ka = int(std::floor((phi + 1.5707963f) / 3.1415927f)), kb = int(std::floor(phi / 3.1415927f));
                const float x = cx + float(i - 1) * 24.f, y = cy + std::sin(T * 2.3f + float(i) * 2.f) * 9.f;
                put(sp, x, y, seq[((ka + i * 2) % 6 + 6) % 6], sa * 0.42f, wob + 0.05f, T + float(i));
                put(sp, x, y, seq[((kb + i * 2 + 3) % 6 + 6) % 6], sb * 0.42f, wob + 0.05f, T + float(i));
            }
            break;
        }
        case ModeId::Blind: {  // an eye that opens and closes over a drop of question marks
            const float lid = 0.15f + 0.85f * std::pow(pulse(std::fmod(T / 3.4f, 1.f)), 0.5f);
            const float ew = 22.f, eh = 11.f * lid;
            g.disc(cx, cy, 3, t.dim);
            for (int x = -int(ew); x <= int(ew); ++x) {
                const float u = float(x) / ew, hh = eh * std::sqrt(std::max(0.f, 1.f - u * u));
                g.rect(cx + float(x), cy - hh, 1, 2.f * hh + 1.f, mix(t.panelHi, t.text, 0.55f));
                g.rect(cx + float(x), cy - hh - 1.f, 1, 1, t.ink);
                g.rect(cx + float(x), cy + hh + 1.f, 1, 1, t.ink);
            }
            const float px = cx + std::sin(T * 0.9f) * 6.f;
            g.disc(px, cy, std::max(1.f, 5.f * lid), t.ink);
            g.disc(px, cy, std::max(0.f, 3.f * lid), t.accent);
            g.text("?", cx, cy - 25.f + std::sin(T * 2.f) * 2.f, 2, withAlpha(t.text, int(120.f + 100.f * std::sin(T * 2.f))), Align::Center);
            break;
        }
        default: break;
    }
}

float modeIconSStep(float e0, float e1, float x) { return sstep(e0, e1, x); }
float modeIconPulse(float p) { return pulse(p); }

}  // namespace liq
