#pragma once
// Unified per-frame input snapshot. Touch is delivered by SDL as mouse events
// (mouse coordinates are already mapped to the logical 360x640 screen).
#include <string>
#include <vector>
namespace liq {

struct Input {
    float mx = 0, my = 0;
    bool down = false;                       // held
    bool pressed = false, released = false;  // edges this frame
    bool left = false, right = false;        // held keys
    float wheel = 0;                         // mouse wheel steps this frame (up = positive)
    bool focusLost = false;                  // window lost focus (sticky: the consumer clears it)
    std::string text;                        // typed characters this frame (SDL text input)
    std::vector<std::string> dropped;        // files dropped on the window this frame (desktop)
    bool backspace = false;                  // backspace edge
    bool confirm = false, esc = false, up = false, dn = false, drop = false, undo = false;  // key edges

    void beginFrame() { text.clear(); dropped.clear(); backspace = false; wheel = 0; pressed = released = confirm = esc = up = dn = drop = undo = false; }
};

}  // namespace liq
