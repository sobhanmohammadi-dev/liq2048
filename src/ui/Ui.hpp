#pragma once
// Immediate-mode widgets drawn in the flat pixel style. Buttons are matched by
// call order within a frame; keyboard focus (up/down/confirm) walks the
// focusable ones.
#include <string>

#include "core/Types.hpp"

namespace liq {

struct Ctx;

class Ui {
public:
    void bind(Ctx* c) { c_ = c; }
    void beginFrame();
    void endFrame();
    bool button(const std::string& label, float x, float y, float w, float h, bool enabled = true,
                bool focusable = true);
    void panel(float x, float y, float w, float h);
    void paragraph(const std::string& s, float x, float y, float w, int scale, Color col, Align a);
    float mx(float x, float w) const;  // mirrors x for RTL layouts
    Align start() const;               // leading alignment (Left, or Right in RTL)
    void resetFocus() { focus_ = 0; }

private:
    Ctx* c_ = nullptr;
    int nextId_ = 0, fidx_ = 0, fcount_ = 0, focus_ = 0, active_ = -1;
};

}  // namespace liq
