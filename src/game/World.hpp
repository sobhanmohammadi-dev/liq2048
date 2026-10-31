#pragma once
// Blob physics. Circles are simulated with position based dynamics (PBD):
// integrate, then iteratively push overlapping circles apart, then derive
// velocity from the position change. This is very stable for piles of circles
// and cheap. The "liquid" feel comes from heavy damping, soft impacts, a
// visual squash/pop on contact and merge, and merge-on-contact.
#include <vector>

#include "core/Config.hpp"

namespace liq {

struct Blob {
    float x = 0, y = 0, vx = 0, vy = 0, px = 0, py = 0;
    int level = 0;
    float r = 0;
    float squash = 0;  // visual only, 0..1
    float pop = 0;     // visual only, 1 right after a merge
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
    void updateOverflow(float deadY, float dt);
    float overflow() const { return overflow_; }
    void resetOverflow() { overflow_ = 0; }
    int highest() const;

    std::vector<Blob> blobs;
    std::vector<MergeEvent> events;  // drained by the game each step

private:
    void substep(float h, float g);
    int chain_ = 0;
    float sinceMerge_ = 99.f;
    float overflow_ = 0.f;
};

}  // namespace liq
