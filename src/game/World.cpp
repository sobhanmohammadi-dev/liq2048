#include "game/World.hpp"

#include <algorithm>
#include <cmath>

namespace liq {

float World::radiusFor(int level) { return cfg::kBaseRadius * std::pow(cfg::kRadiusGrowth, float(level)); }

void World::clear() {
    blobs.clear();
    events.clear();
    chain_ = 0;
    spawned_ = 0;
    sinceMerge_ = 99.f;
    overflow_ = 0.f;
}

void World::spawn(float x, float y, int level) {
    Blob b;
    b.x = b.px = b.ox = x;
    b.y = b.py = b.oy = y;
    b.level = level;
    b.r = radiusFor(level);
    b.rs = -0.28f;  // starts small and pops out like a droplet forming
    b.rsv = 7.f;
    b.phase = float(spawned_++) * 2.399963f;  // golden angle: neighbours breathe out of sync
    blobs.push_back(b);
}

int World::highest() const {
    int h = -1;
    for (const auto& b : blobs) h = std::max(h, b.level);
    return h;
}

void World::step(float dt, float gravity) {
    for (auto& b : blobs) {
        b.ox = b.x;
        b.oy = b.y;
    }
    const float h = dt / cfg::kSubsteps;
    for (int s = 0; s < cfg::kSubsteps; ++s) substep(h, gravity);
    for (auto& b : blobs) b.age += dt;
    sinceMerge_ += dt;
}

void World::animate(float dt) {
    clock_ += dt;
    for (auto& b : blobs) {
        // Impact kicks become shear velocity, then a damped spring wobbles it back to round.
        b.dsa += b.ka * vis::kImpactKick;
        b.dsb += b.kb * vis::kImpactKick;
        b.ka = b.kb = 0.f;
        b.dsa += (-vis::kShearSpring * b.sa - vis::kShearDamp * b.dsa) * dt;
        b.dsb += (-vis::kShearSpring * b.sb - vis::kShearDamp * b.dsb) * dt;
        b.sa += b.dsa * dt;
        b.sb += b.dsb * dt;
        const float m = std::hypot(b.sa, b.sb);
        if (m > vis::kMaxShear) {
            b.sa *= vis::kMaxShear / m;
            b.sb *= vis::kMaxShear / m;
        }
        b.rsv += (-vis::kPopSpring * b.rs - vis::kPopDamp * b.rsv) * dt;
        b.rs += b.rsv * dt;
    }
}

BlobSprite World::sprite(const Blob& b, float alpha) const {
    BlobSprite s;
    s.x = b.ox + (b.x - b.ox) * alpha;
    s.y = b.oy + (b.y - b.oy) * alpha;
    s.level = b.level;
    const float base = b.r * (1.f + b.rs + vis::kBreath * std::sin(clock_ * 6.2831853f * vis::kBreathHz + b.phase));

    // Shear = spring wobble + stretch along the direction of motion.
    float A = b.sa, B = b.sb;
    const float sp2 = b.vx * b.vx + b.vy * b.vy;
    if (sp2 > 1.f) {
        const float sp = std::sqrt(sp2);
        const float vs = std::min(0.22f, sp * vis::kSpeedStretch);
        A += vs * (b.vx * b.vx - b.vy * b.vy) / sp2;
        B += vs * (2.f * b.vx * b.vy) / sp2;
    }
    const float lam = std::min(std::hypot(A, B), vis::kMaxShear);
    if (lam < 1e-4f) {
        s.ra = s.rb = base;
        return s;
    }
    const float c2 = A / std::hypot(A, B), s2 = B / std::hypot(A, B);
    s.cs = std::sqrt((1.f + c2) * 0.5f);
    s.sn = std::copysign(std::sqrt((1.f - c2) * 0.5f), s2);
    s.ra = base * (1.f + lam);  // volume preserving: ra * rb = base^2
    s.rb = base / (1.f + lam);
    return s;
}

void World::substep(float h, float g) {
    for (auto& b : blobs) {
        b.px = b.x;
        b.py = b.y;
        b.vy += g * h;
        b.x += b.vx * h;
        b.y += b.vy * h;
    }

    for (int it = 0; it < cfg::kIterations; ++it) {
        for (auto& b : blobs) {  // container walls
            b.x = std::clamp(b.x, cfg::kFieldL + b.r, cfg::kFieldR - b.r);
            if (b.y > cfg::kFloor - b.r) b.y = cfg::kFloor - b.r;
        }
        for (size_t i = 0; i < blobs.size(); ++i) {
            for (size_t j = i + 1; j < blobs.size(); ++j) {
                Blob &a = blobs[i], &b = blobs[j];
                float dx = b.x - a.x, dy = b.y - a.y;
                const float minD = a.r + b.r;
                const float d2 = dx * dx + dy * dy;
                if (d2 >= minD * minD) continue;
                float d = std::sqrt(d2);
                if (d < 1e-4f) {
                    dx = 1e-4f;
                    dy = 0.f;
                    d = 1e-4f;
                }
                const float overlap = minD - d;
                // Heavier (bigger) blobs move less.
                const float wa = 1.f / (a.r * a.r), wb = 1.f / (b.r * b.r);
                const float k = overlap / (wa + wb) / d;
                a.x -= dx * k * wa;
                a.y -= dy * k * wa;
                b.x += dx * k * wb;
                b.y += dy * k * wb;
            }
        }
    }

    for (auto& b : blobs) {  // derive velocity; hard impacts kick the jelly spring
        const float nvx = (b.x - b.px) / h, nvy = (b.y - b.py) / h;
        const float dvx = nvx - b.vx, dvy = nvy - b.vy;
        const float dv = std::hypot(dvx, dvy);
        if (dv > 70.f) {
            // Flatten along the impact direction: in pure-shear space that is -(cos2φ, sin2φ)·|dv|.
            b.ka += -(dvx * dvx - dvy * dvy) / dv;
            b.kb += -(2.f * dvx * dvy) / dv;
        }
        const bool onFloor = b.y >= cfg::kFloor - b.r - 0.01f;
        b.vx = nvx * cfg::kDamping * (onFloor ? 0.96f : 1.f);
        b.vy = nvy * cfg::kDamping;
        const float sp = std::hypot(b.vx, b.vy);
        if (sp > cfg::kMaxSpeed) {
            b.vx *= cfg::kMaxSpeed / sp;
            b.vy *= cfg::kMaxSpeed / sp;
        }
    }

    // Merge pass: same-level blobs in contact fuse into the next level.
    std::vector<Blob> born;
    for (size_t i = 0; i < blobs.size(); ++i) {
        if (blobs[i].dead) continue;
        for (size_t j = i + 1; j < blobs.size(); ++j) {
            Blob &a = blobs[i], &b = blobs[j];
            if (b.dead || a.level != b.level || a.level >= cfg::kMaxLevel) continue;
            const float d = std::hypot(b.x - a.x, b.y - a.y);
            if (d > a.r + b.r + 1.f) continue;
            const float ma = a.r * a.r, mb = b.r * b.r;
            Blob nb;
            nb.level = a.level + 1;
            nb.r = radiusFor(nb.level);
            nb.x = nb.px = nb.ox = (a.x * ma + b.x * mb) / (ma + mb);
            nb.y = nb.py = nb.oy = (a.y * ma + b.y * mb) / (ma + mb);
            nb.vx = (a.vx + b.vx) * 0.5f;
            nb.vy = (a.vy + b.vy) * 0.5f;
            nb.sa = (a.sa + b.sa) * 0.5f;
            nb.sb = (a.sb + b.sb) * 0.5f;
            nb.dsa = (a.dsa + b.dsa) * 0.5f;
            nb.dsb = (a.dsb + b.dsb) * 0.5f;
            nb.rs = -0.20f;  // pop: start a little small, overshoot, settle
            nb.rsv = 8.f;
            nb.phase = a.phase;
            nb.age = 0.8f;  // short grace period before it can count as overflow
            a.dead = b.dead = true;
            chain_ = sinceMerge_ < cfg::kChainWindow ? chain_ + 1 : 1;
            sinceMerge_ = 0.f;
            events.push_back({nb.x, nb.y, nb.level, chain_});
            born.push_back(nb);
            break;
        }
    }
    if (!born.empty()) {
        blobs.erase(std::remove_if(blobs.begin(), blobs.end(), [](const Blob& b) { return b.dead; }), blobs.end());
        blobs.insert(blobs.end(), born.begin(), born.end());
    }
}

void World::updateOverflow(float deadY, float dt) {
    bool over = false;
    for (const auto& b : blobs)
        if (b.age > 1.2f && b.y - b.r < deadY && std::hypot(b.vx, b.vy) < 60.f) over = true;
    overflow_ = over ? overflow_ + dt : std::max(0.f, overflow_ - dt * 2.f);
}

}  // namespace liq
