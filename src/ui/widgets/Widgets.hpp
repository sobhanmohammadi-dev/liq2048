#pragma once
// Stateless drawing widgets shared by the HUD, overlays and settings pages.
//
// Every function draws immediately with the given Gfx and Theme and keeps no state, so a scene may call
// it any number of times per frame. Interactive controls (buttons, steppers, panels) live in ui/Ui.hpp;
// this module holds the purely visual building blocks around them. All coordinates are in logical
// pixels (cfg::kW x cfg::kH) and every widget snaps to whole pixels to keep the pixel-art look.
#include <string>
#include <vector>

#include "core/Themes.hpp"
#include "core/Types.hpp"
#include "gfx/Gfx.hpp"

namespace liq::widgets {

// Horizontal meter: ink frame, dark trough, accent fill. `frac` is clamped to 0..1.
void meter(Gfx& g, const Theme& t, float x, float y, float w, float h, float frac);

// Row of small blocks, the first round(frac * count) lit (mode progress). Centred on the screen.
void segments(Gfx& g, const Theme& t, float y, int count, float frac);

// Page indicator: `count` squares centred on cx, the active one in the accent colour.
void pageDots(Gfx& g, const Theme& t, float cx, float y, int count, int active);

// Four-square "waiting" indicator that steps with `time` (seconds).
void spinner(Gfx& g, const Theme& t, float cx, float y, float time);

// The twelve-colour droplet palette as framed squares starting at x.
void swatches(Gfx& g, const Theme& t, float x, float y, int count, float cell = 16.f, float pitch = 18.f);

// Bottom strip showing which droplet levels have been reached; `maxLevel` is marked with a pointer.
void levelStrip(Gfx& g, const Theme& t, float y, int maxLevel);

// Coaching banner: a framed box of wrapped text, horizontally centred at y.
void banner(Gfx& g, const Theme& t, float y, float w, const std::string& text);

// Toast: slides down from the top while `timeLeft` runs; `total` is its full duration in seconds.
void toast(Gfx& g, const Theme& t, const std::string& text, float timeLeft, float total);

// Overlay slide-in: returns the y offset (<= 0) of a panel `t` seconds after it appeared.
float slideIn(float t);

// A framed disc (ink ring + fill) used for droplet icons.
void chip(Gfx& g, const Theme& t, float x, float y, float r, Color fill);

// Crown flanking a centred caption, e.g. "NEW BEST".
void crownCaption(Gfx& g, const Theme& t, float cx, float y, const std::string& text);

}  // namespace liq::widgets
