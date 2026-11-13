#pragma once
// Immediate-mode widgets drawn in the pixel-art style. Buttons are matched by call order
// within a frame; keyboard focus (up/down/confirm) walks the focusable ones.
#include <string>
#include <vector>

#include "core/Types.hpp"
#include "gfx/Icons.hpp"

namespace liq {

struct Ctx;

enum class BtnStyle { Normal, Primary };

class Ui {
public:
    void bind(Ctx* c) { c_ = c; }
    void beginFrame();
    void endFrame();
    bool button(const std::string& label, float x, float y, float w, float h, bool enabled = true,
                bool focusable = true, BtnStyle style = BtnStyle::Normal);
    bool iconButton(IconId icon, float x, float y, float w, float h, bool enabled = true, bool focusable = false);
    // [<] value [>] control. Returns -1, 0 or +1.
    int stepper(float x, float y, float w, float h, const std::string& value, bool bitmapValue = false);
    void panel(float x, float y, float w, float h);
    void paragraph(const std::string& s, float x, float y, float w, int scale, Color col, Align a);
    float mx(float x, float w) const;  // mirrors x for RTL layouts (and left-handed HUD)
    void setMirror(bool on) { mirror_ = on; }
    bool mirrored() const { return mirror_; }  // extra mirroring (left-handed game layout)
    Align start() const;               // leading alignment (Left, or Right in RTL)
    void resetFocus() { focus_ = 0; }
    // True when (x, y) is on a widget drawn in this or the previous frame (including its touch margin): the game uses it
    // so that a tap on a button never also aims or drops.
    bool overWidget(float x, float y) const;

private:
    struct Hit {
        bool hover, held, click, focus;
    };
    Hit hit(float x, float y, float w, float h, bool enabled, bool focusable);
    Ctx* c_ = nullptr;
    bool mirror_ = false;
    int nextId_ = 0, fidx_ = 0, focus_ = 0, active_ = -1;
    int bestId_ = -1;           // the widget a press landed on best (see hit)
    float bestScore_ = 1e9f;
    struct Rect { float x0, y0, x1, y1; };
    std::vector<Rect> rects_, prevRects_;
};

}  // namespace liq
