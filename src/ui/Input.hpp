#pragma once
// Unified per-frame input snapshot. Touch is delivered by SDL as mouse events
// (mouse coordinates are already mapped to the logical 360x640 screen).
namespace liq {

struct Input {
    float mx = 0, my = 0;
    bool down = false;                       // held
    bool pressed = false, released = false;  // edges this frame
    bool left = false, right = false;        // held keys
    float wheel = 0;                         // mouse wheel steps this frame (up = positive)
    bool focusLost = false;                  // window lost focus (sticky: the consumer clears it)
    bool confirm = false, esc = false, up = false, dn = false, drop = false, undo = false;  // key edges

    void beginFrame() { wheel = 0; pressed = released = confirm = esc = up = dn = drop = undo = false; }
};

}  // namespace liq
