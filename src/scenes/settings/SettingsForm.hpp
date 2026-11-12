#pragma once
// Layout cursor and row widgets shared by every Settings page.
//
// A page receives a SettingsForm positioned at its first row and draws top to bottom: each row call
// draws a label on the leading side and a control in the control column, then advances the cursor by
// one row. The form hides the RTL / left-hand mirroring so a page never thinks about it.
#include <string>

#include "scenes/Scenes.hpp"

namespace liq::settings {

constexpr float kRowH = 32.f;

struct SettingsForm {
    SettingsForm(Ctx& ctx, float firstRowY);

    Ctx& c;
    float y;        // top of the next row
    float lx;       // label anchor (x)
    Align la;       // label alignment
    float cx, cw;   // control column (already mirrored)

    void label(const char* key) const;                      // label of the current row
    void skip(float h) { y += h; }                           // vertical gap
    void next() { y += kRowH; }                              // advance one row

    // ON/OFF button row bound to a bool; persists the save on change. Returns true if toggled.
    bool toggle(const char* key, bool& v);
    // [<] value [>] row. Returns -1, 0 or +1. `bitmapValue` draws the value with the bitmap font (digits, names).
    int stepper(const char* key, const std::string& value, bool bitmapValue = false);
    // Percentage stepper in 10 % steps, 0..100 %; returns true when the value changed.
    bool percent(const char* key, float& v);
    // Small labelled meter (liquid property bars). `frac` is clamped to 0..1.
    void meterRow(const char* key, float frac);
    // Centred, wrapped explanatory text below the rows.
    void note(const std::string& text, float gapAfter);
};

// One function per page. Each draws its rows; navigation (Back / next page) is drawn by drawSettings().
void generalPage(SettingsForm& f, bool allowCustom);
void feelPage(SettingsForm& f);
void accessPage(SettingsForm& f);
void liquidPage(SettingsForm& f);

}  // namespace liq::settings
