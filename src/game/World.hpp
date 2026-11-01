#pragma once
// Blob physics. Circles are simulated with position based dynamics (PBD):
// integrate, then iteratively push overlapping circles apart, then derive
// velocity from the position change. This is very stable for piles of circles
// and cheap. The physics circles stay rigid; the liquid look is a visual layer:
//   * a damped "jelly" spring per blob (sa, sb) that is kicked by impacts and turns
//     the drawn circle into a wobbling ellipse,
//   * a pop spring (rs) used when a blob is born or merged,
//   * gentle idle breathing and velocity stretch applied in sprite().
// step() runs at a fixed 60 Hz; animate() runs every rendered frame so the wobble is
// smooth at any frame rate, and sprite() interpolates positions between steps.
#include <vector>

#include "core/Config.hpp"
#include "core/Types.hpp"

namespace liq {

struct Blob {
    float x = 0, y = 0, vx = 0, vy = 0, px = 0, py = 0;
    float ox = 0, oy = 0;          // position at the start of the last fixed step (interpolation)
    int level = 0;
    float r = 0;
    float sa = 0, sb = 0;          // jelly shear spring (pure shear: a = cos2θ·λ, b = sin2θ·λ)
    float dsa = 0, dsb = 0;        // ... and its velocity
    float ka = 0, kb = 0;          // impact kicks accumulated by the solver
    float rs = 0, rsv = 0;         // pop spring (radius multiplier offset)
    float phase = 0;               // breathing phase
    float age = 0;
    bool dead = false;
};

struct MergeEvent {
    float x, y;
    int level;  // level of the NEW blob
    int chain;  // 1 = single merge, 2+ = chained within kChainWindow
};

class World {
public:
    static float radiusFor(int level);
    void clear();
    void spawn(float x, float y, int level);
    void step(float dt, float gravity);           // one fixed frame (substeps inside)
    void animate(float dt);                       // visual springs; call every rendered frame
    void updateOverflow(float deadY, float dt);
    float overflow() const { return overflow_; }
    void resetOverflow() { overflow_ = 0; }
    int highest() const;
    // Drawn shape of a blob; alpha in [0,1] interpolates between the last two fixed steps.
    BlobSprite sprite(const Blob& b, float alpha) const;
    float clock() const { return clock_; }

    std::vector<Blob> blobs;
    std::vector<MergeEvent> events;  // drained by the game each step

private:
    void substep(float h, float g);
    int chain_ = 0;
    int spawned_ = 0;
    float sinceMerge_ = 99.f;
    float overflow_ = 0.f;
    float clock_ = 0.f;
};

}  // namespace liq
