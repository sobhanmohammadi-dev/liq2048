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
    float rx = 0, ry = 0;  // rest offset from the drop's centroid (jelly liquids spring back to it)
    int g = 0;             // owning drop index
    uint8_t mat = 0;       // Liquid mixing: material of this particle (index into World::mix)
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
    void clear();                            // keeps the container geometry
    void spawn(float x, float y, int level, int mat = 0);
    void step(float dt, float gravity, float gx = 0.f);  // one fixed frame (substeps inside); gx = sideways acceleration (wind)
    void updateOverflow(float deadY, float dt);
    float overflow() const { return overflow_; }
    void resetOverflow() { overflow_ = 0; }
    int highest() const;
    // Remove drops that rest above the line (Zen mode); returns their centroids and levels.
    std::vector<Drop> popAbove(float deadY);
    // Height of the liquid surface at x (for the aim guide); floor if empty.
    float surfaceY(float x, float halfWidth) const;

    // Battle spectating: a compact picture of the pot (drops with their particles, positions in 1/128 px) and the
    // reverse, which rebuilds a World that can only be drawn (never stepped).
    void picture(std::vector<int>& levels, std::vector<int>& counts, std::vector<uint16_t>& pos) const;
    void setPicture(const std::vector<int>& levels, const std::vector<int>& counts, const std::vector<uint16_t>& pos);

    // Rendering helpers. alpha in [0,1] interpolates between the last two fixed steps.
    void particleSprites(float alpha, float radius, std::vector<BlobSprite>& out) const;
    void labelSprites(std::vector<BlobSprite>& out) const;

    // Container geometry. The defaults are the normal pot; bigger or smaller pots (Big Container, Mirror) change them.
    float fieldL = cfg::kFieldL, fieldR = cfg::kFieldR, floorY = cfg::kFloor;
    float worldW = float(cfg::kW), worldH = float(cfg::kH);  // extent of the spatial hash (must cover every particle)
    // Re-seeds drop i at the same centroid with another level (Madhouse): the volume and the colour follow the level.
    void relevel(int i, int level);
    int substeps = cfg::kSubsteps;   // 1..3, the "Liquid quality" setting (2 = reference physics)
    float viscScale = 1.f;           // viscosity multiplier (liquid type x Thick modifier)
    // Material (see game/Liquid.hpp): surface tension while moving / at rest, wall friction and damping per substep.
    float jellyDamp = cfg::kJellyDamp;  // ... and how strongly motion inside the drop is damped (1/s)
    float jelly = 0.f;  // shape memory (spring, 1/s^2; 0 = none): a drop springs back to its round shape and wobbles
    float shear = 0.f;  // velocity averaging between neighbours (0 = none): the liquid moves as one gooey mass
    float wallVy = 0.9f;  // vertical velocity kept on touching the floor or a wall (low = the liquid lands with a plop)
    // Liquid mixing (v1.8.4): every drop is its own liquid. When `mixed`, viscosity, surface tension and wall friction come
    // from the particles' material instead of the world-wide values above.
    struct MixMat { float visc = 1.f, coh = 1.f, rest = 0.f, drag = 0.97f, vy = 0.9f; };
    bool mixed = false;
    MixMat mix[8];
    float cohesion = 1.f, restCohesion = 0.f, wallDrag = cfg::kWallDrag, damp = cfg::kDamp;

    std::vector<Particle> parts;
    std::vector<Drop> drops;
    std::vector<MergeEvent> events;  // drained by the game each step
    std::vector<Impact> impacts;     // liquid landing events, drained by the game each step

private:
    void substep(float h, float g, bool last);
    float gx_ = 0.f;
    std::vector<float> jc_;  // scratch: centroid sums per drop (jelly)
    void refresh();
    void anchors();
    void mergeGroups(int a, int b);
    void removeDrop(int g);
    int chain_ = 0;
    float sinceMerge_ = 99.f;
    float overflow_ = 0.f;
    // uint32_t seed_ = 1;
    // Spatial hash scratch.
    struct Pos { float x, y; };
    void gatherPositions();
    std::vector<Pos> pos_;  // packed copy of the particle positions (cache friendly pair loops)
    std::vector<int> cellStart_, cellOf_, fill_, sorted_;  // counting-sort cell grid of the pair search
    std::vector<Pos> sortedPos_;
    struct PairRec {
        int i, j;
        float q, nx, ny;  // distance / radius and unit vector i -> j
    };
    std::vector<PairRec> pairs_;  // active neighbour pairs (q < 1) for the current positions
    std::vector<PairRec> cand_;   // candidate pairs found with a margin, valid for one fixed step
    bool candValid_ = false;
    std::vector<float> rho_, rhoN_, rhoG_, press_;
    std::vector<char> moving_;
    int stepCount_ = 0;
    void buildPairs(float margin = 0.f);
    void regeom();
};

}  // namespace liq
