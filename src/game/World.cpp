#include "game/World.hpp"

#include <algorithm>
#include <cmath>

namespace liq {

float World::radiusFor(int level) { return cfg::kBaseRadius * std::pow(cfg::kRadiusGrowth, float(level)); }

void World::clear() {
    blobs.clear();
    events.clear();
    chain_ = 0;
    sinceMerge_ = 99.f;
    overflow_ = 0.f;
}

void World::spawn(float x, float y, int level) {
    Blob b;
    b.x = b.px = x;
    b.y = b.py = y;
    b.level = level;
    b.r = radiusFor(level);
    blobs.push_back(b);
}

int World::highest() const {
    int h = -1;
    for (const auto& b : blobs) h = std::max(h, b.level);
    return h;
}

void World::step(float dt, float gravity) {
    const float h = dt / cfg::kSubsteps;
    for (int s = 0; s < cfg::kSubsteps; ++s) substep(h, gravity);
    for (auto& b : blobs) {
        b.squash = std::max(0.f, b.squash - dt * 4.f);
        b.pop = std::max(0.f, b.pop - dt * 5.f);
        b.age += dt;
    }
    sinceMerge_ += dt;
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
                float minD = a.r + b.r;
                float d2 = dx * dx + dy * dy;
                if (d2 >= minD * minD) continue;
                float d = std::sqrt(d2);
                if (d < 1e-4f) {
                    dx = 1.f;
                    dy = 0.f;
                    d = 1e-4f;
                    dx = 1e-4f;
                }
                float overlap = minD - d;
                // Heavier (bigger) blobs move less.
                float wa = 1.f / (a.r * a.r), wb = 1.f / (b.r * b.r);
                float k = overlap / (wa + wb) / d;
                a.x -= dx * k * wa;
                a.y -= dy * k * wa;
                b.x += dx * k * wb;
                b.y += dy * k * wb;
            }
        }
    }

    for (auto& b : blobs) {  // derive velocity, add visual squash on hard impacts
        float nvx = (b.x - b.px) / h, nvy = (b.y - b.py) / h;
        float dv = std::hypot(nvx - b.vx, nvy - b.vy);
        if (dv > 150.f) b.squash = std::max(b.squash, std::min(1.f, dv / 700.f));
        bool onFloor = b.y >= cfg::kFloor - b.r - 0.01f;
        b.vx = nvx * cfg::kDamping * (onFloor ? 0.96f : 1.f);
        b.vy = nvy * cfg::kDamping;
        float sp = std::hypot(b.vx, b.vy);
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
            float d = std::hypot(b.x - a.x, b.y - a.y);
            if (d > a.r + b.r + 1.f) continue;
            float ma = a.r * a.r, mb = b.r * b.r;
            Blob nb;
            nb.level = a.level + 1;
            nb.r = radiusFor(nb.level);
            nb.x = nb.px = (a.x * ma + b.x * mb) / (ma + mb);
            nb.y = nb.py = (a.y * ma + b.y * mb) / (ma + mb);
            nb.vx = (a.vx + b.vx) * 0.5f;
            nb.vy = (a.vy + b.vy) * 0.5f;
            nb.pop = 1.f;
            nb.squash = 0.5f;
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
        if (b.age > 1.2f && b.y - b.r < deadY && std::hypot(b.vx, b.vy) < 80.f) over = true;
    overflow_ = over ? overflow_ + dt : std::max(0.f, overflow_ - dt * 2.f);
}

}  // namespace liq
