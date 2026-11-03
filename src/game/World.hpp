#pragma once
// Liquid simulation. Every droplet is a cluster of fluid particles (double-density
// relaxation, Clavet et al. 2005, in a position-based form):
//   * pressure keeps the liquid at a constant density, so a drop splashes on impact,
//     spreads over the floor and levels out in the pot instead of staying a ball,
//   * viscosity makes it thick like syrup rather than watery,
//   * a light same-colour cohesion keeps a falling drop together and each colour in its
//     own region (the liquids touch and blend visually, but do not truly mix).
// Particles carry a group (drop) id. Two groups of the same level that touch merge into the
// next level. step() runs at a fixed 60 Hz; particles are interpolated between steps.
#include <cstdint>
#include <vector>

#include "core/Config.hpp"
#include "core/Types.hpp"

namespace liq {

struct Particle {
    float x = 0, y = 0, vx = 0, vy = 0, px = 0, py = 0;
    float ox = 0, oy = 0;  // position at the start of the last fixed step (interpolation)
    int g = 0;             // owning drop index
};

// One drop of liquid (a group of particles). Derived fields are refreshed every step.
struct Drop {
    int level = 0;
    int n = 0;                       // particle count
    float x = 0, y = 0, vx = 0, vy = 0;  // centroid and mean velocity
    float top = 0;                   // highest point (smallest y)
    float sxx = 0, sxy = 0, syy = 0; // position covariance
    float lx = 0, ly = 0, lr = 0;    // where the number is drawn (inside the liquid body) and its size
    float pvy = 0;                   // mean vertical speed last step (impact detection)
    float age = 0;
    float pop = 0, popv = 0;         // label/size pop spring after a merge or spawn
    bool dead = false;
};

struct Impact {
    float x, y;
    float strength;  // 0..1 (speed x size)
    int level;
    uint32_t color;  // unused placeholder for future effects
};

struct MergeEvent {
    float x, y;
    int level;  // level of the NEW drop
    int chain;  // 1 = single merge, 2+ = chained within kChainWindow
};

class World {
public:
    static float radiusFor(int level);       // nominal radius of a round drop of that level
    static int particlesFor(int level);      // particle count (constant volume per level)
    void clear();
    void spawn(float x, float y, int level);
    void step(float dt, float gravity);      // one fixed frame (substeps inside)
    void updateOverflow(float deadY, float dt);
    float overflow() const { return overflow_; }
    void resetOverflow() { overflow_ = 0; }
    int highest() const;
    // Remove drops that rest above the line (Zen mode); returns their centroids and levels.
    std::vector<Drop> popAbove(float deadY);
    // Height of the liquid surface at x (for the aim guide); floor if empty.
    float surfaceY(float x, float halfWidth) const;

    // Rendering helpers. alpha in [0,1] interpolates between the last two fixed steps.
    void particleSprites(float alpha, float radius, std::vector<BlobSprite>& out) const;
    void labelSprites(std::vector<BlobSprite>& out) const;

    float viscScale = 1.f;           // Thick liquid modifier multiplies the viscosity

    std::vector<Particle> parts;
    std::vector<Drop> drops;
    std::vector<MergeEvent> events;  // drained by the game each step
    std::vector<Impact> impacts;     // liquid landing events, drained by the game each step

private:
    void substep(float h, float g, bool last);
    void refresh();
    void anchors();
    void mergeGroups(int a, int b);
    void removeDrop(int g);
    int chain_ = 0;
    float sinceMerge_ = 99.f;
    float overflow_ = 0.f;
    uint32_t seed_ = 1;
    // Spatial hash scratch.
    std::vector<int> head_, next_;
    struct PairRec {
        int i, j;
        float q, nx, ny;  // distance / radius and unit vector i -> j
    };
    std::vector<PairRec> pairs_;  // active neighbour pairs (q < 1) for the current positions
    std::vector<PairRec> cand_;   // candidate pairs found with a margin, valid for one fixed step
    bool candValid_ = false;
    std::vector<float> rho_, rhoN_, rhoG_;
    int stepCount_ = 0;
    void buildPairs(float margin = 0.f);
    void regeom();
};

}  // namespace liq
