#pragma once
// Unified per-frame input snapshot. Touch is delivered by SDL as mouse events
// (mouse coordinates are already mapped to the logical 360x640 screen).
namespace liq {

struct Input {
    float mx = 0, my = 0;
    bool down = false;                       // held
    bool pressed = false, released = false;  // edges this frame
    bool left = false, right = false;        // held keys
    bool confirm = false, esc = false, up = false, dn = false, drop = false;  // key edges

    void beginFrame() { pressed = released = confirm = esc = up = dn = drop = false; }
};

}  // namespace liq
