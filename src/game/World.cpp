#include "game/World.hpp"

#include <algorithm>
#include <cmath>

namespace liq {

namespace {

constexpr float kPi = 3.14159265f;
constexpr float kPartArea = 0.8660254f * cfg::kSpacing * cfg::kSpacing;  // hex packing

// Density of a perfect hex lattice at rest spacing: the target of the pressure solver.
float restDensity(float& restNear) {
    float rho = 0.f, rn = 0.f;
    for (int a = -4; a <= 4; ++a)
        for (int b = -4; b <= 4; ++b) {
            if (!a && !b) continue;
            const float x = (float(a) + float(b) * 0.5f) * cfg::kSpacing, y = float(b) * 0.8660254f * cfg::kSpacing;
            const float q = std::hypot(x, y) / cfg::kInteract;
            if (q >= 1.f) continue;
            rho += (1.f - q) * (1.f - q);
            rn += (1.f - q) * (1.f - q) * (1.f - q);
        }
    restNear = rn;
    return rho;
}

}  // namespace

float World::radiusFor(int level) { return cfg::kBaseRadius * std::pow(cfg::kRadiusGrowth, float(level)); }

int World::particlesFor(int level) {
    const float r = radiusFor(level);
    return std::max(3, int(std::lround(kPi * r * r / kPartArea)));
}

void World::clear() {
    parts.clear();
    drops.clear();
    events.clear();
    chain_ = 0;
    sinceMerge_ = 99.f;
    overflow_ = 0.f;
}

void World::spawn(float x, float y, int level) {
    const float r = radiusFor(level);
    Drop d;
    d.level = level;
    const int gi = int(drops.size());
    const float dx = cfg::kSpacing, dy = cfg::kSpacing * 0.8660254f;
    const int rows = int(r / dy) + 1;
    for (int j = -rows; j <= rows; ++j)
        for (int i = -int(r / dx) - 2; i <= int(r / dx) + 2; ++i) {
            const float px = x + (float(i) + ((j & 1) ? 0.5f : 0.f)) * dx, py = y + float(j) * dy;
            if (std::hypot(px - x, py - y) > r - 0.5f * cfg::kSpacing * 0.5f) continue;
            Particle p;
            p.x = p.px = p.ox = px;
            p.y = p.py = p.oy = py;
            p.g = gi;
            parts.push_back(p);
            ++d.n;
        }
    d.x = x;
    d.y = y;
    d.top = y - r;
    drops.push_back(d);
}

int World::highest() const {
    int h = -1;
    for (const auto& d : drops) h = std::max(h, d.level);
    return h;
}

void World::refresh() {
    for (auto& d : drops) {
        d.n = 0;
        d.x = d.y = d.vx = d.vy = 0.f;
        d.top = 1e9f;
        d.sxx = d.sxy = d.syy = 0.f;
    }
    for (const auto& p : parts) {
        Drop& d = drops[size_t(p.g)];
        ++d.n;
        d.x += p.x;
        d.y += p.y;
        d.vx += p.vx;
        d.vy += p.vy;
        d.top = std::min(d.top, p.y);
    }
    for (auto& d : drops)
        if (d.n) {
            const float k = 1.f / float(d.n);
            d.x *= k;
            d.y *= k;
            d.vx *= k;
            d.vy *= k;
        }
    for (const auto& p : parts) {
        Drop& d = drops[size_t(p.g)];
        const float ex = p.x - d.x, ey = p.y - d.y;
        d.sxx += ex * ex;
        d.sxy += ex * ey;
        d.syy += ey * ey;
    }
    for (auto& d : drops)
        if (d.n) {
            const float k = 1.f / float(d.n);
            d.sxx *= k;
            d.sxy *= k;
            d.syy *= k;
        }
}

void World::buildPairs(float margin) {
    const float R = cfg::kInteract + margin;
    const float cell = R * 0.5f;
    const int nx = int(float(cfg::kW) / cell) + 5, ny = int(float(cfg::kH) / cell) + 5;
    head_.assign(size_t(nx) * size_t(ny), -1);
    next_.assign(parts.size(), -1);
    auto cellIdx = [&](const Particle& p, int& cx, int& cy) {
        cx = std::clamp(int(p.x / cell) + 2, 0, nx - 1);
        cy = std::clamp(int(p.y / cell) + 2, 0, ny - 1);
    };
    for (int i = 0; i < int(parts.size()); ++i) {
        int cx, cy;
        cellIdx(parts[size_t(i)], cx, cy);
        next_[size_t(i)] = head_[size_t(cy) * size_t(nx) + size_t(cx)];
        head_[size_t(cy) * size_t(nx) + size_t(cx)] = i;
    }
    cand_.clear();
    auto test = [&](int i, int j) {
        const Particle &a = parts[size_t(i)], &b = parts[size_t(j)];
        const float dx = b.x - a.x, dy = b.y - a.y;
        if (dx * dx + dy * dy >= R * R) return;
        cand_.push_back({i, j, 0.f, dx >= 0.f ? 1.f : -1.f, 0.f});  // geometry filled by regeom()
    };
    // Half-shell of neighbour cells within +-2.
    static const int off[13][2] = {{0, 0}, {1, 0}, {2, 0}, {-2, 1}, {-1, 1}, {0, 1}, {1, 1}, {2, 1},
                                   {-2, 2}, {-1, 2}, {0, 2}, {1, 2}, {2, 2}};
    for (int cy = 0; cy < ny; ++cy)
        for (int cx = 0; cx < nx; ++cx) {
            const int first = head_[size_t(cy) * size_t(nx) + size_t(cx)];
            if (first < 0) continue;
            for (int o = 0; o < 13; ++o) {
                const int ax = cx + off[o][0], ay = cy + off[o][1];
                if (ax < 0 || ax >= nx || ay >= ny) continue;
                for (int i = first; i >= 0; i = next_[size_t(i)]) {
                    int j = o == 0 ? next_[size_t(i)] : head_[size_t(ay) * size_t(nx) + size_t(ax)];
                    for (; j >= 0; j = next_[size_t(j)]) test(i, j);
                }
            }
        }
    candValid_ = true;
}

// Recompute pair geometry after particles moved; drop pairs that left the interaction radius.
void World::regeom() {
    pairs_.clear();
    const float R2 = cfg::kInteract * cfg::kInteract;
    for (const PairRec& c : cand_) {
        const Particle &a = parts[size_t(c.i)], &b = parts[size_t(c.j)];
        const float dx = b.x - a.x, dy = b.y - a.y, d2 = dx * dx + dy * dy;
        if (d2 >= R2) continue;
        PairRec pr = c;
        float d = std::sqrt(d2);
        if (d < 1e-4f) {  // exactly stacked: separate along a fixed axis
            pr.nx = (c.i + c.j) & 1 ? 1.f : -0.6f;
            pr.ny = (c.i * 7 + c.j) & 1 ? 0.8f : -0.8f;
            const float l = std::hypot(pr.nx, pr.ny);
            pr.nx /= l;
            pr.ny /= l;
            d = 1e-4f;
        } else {
            pr.nx = dx / d;
            pr.ny = dy / d;
        }
        pr.q = d / cfg::kInteract;
        pairs_.push_back(pr);
    }
}

void World::step(float dt, float gravity) {
    for (auto& p : parts) {
        p.ox = p.x;
        p.oy = p.y;
    }
    const float h = dt / float(cfg::kSubsteps);
    buildPairs(3.f);  // candidate pairs with a margin, reused by every substep
    for (int s = 0; s < cfg::kSubsteps; ++s) substep(h, gravity, s == cfg::kSubsteps - 1);
    for (auto& d : drops) d.age += dt;
    refresh();
    sinceMerge_ += dt;
    if ((++stepCount_ & 1) == 0) anchors();
}

// Pick where each number goes: the middle of the deepest part of the liquid body.
void World::anchors() {
    if (!candValid_) buildPairs(3.f);
    regeom();
    rhoG_.assign(parts.size(), 0.f);
    for (const auto& pr : pairs_) {
        if (parts[size_t(pr.i)].g != parts[size_t(pr.j)].g) continue;
        const float w = (1.f - pr.q) * (1.f - pr.q);
        rhoG_[size_t(pr.i)] += w;
        rhoG_[size_t(pr.j)] += w;
    }
    std::vector<float> best(drops.size(), 0.f);
    for (size_t i = 0; i < parts.size(); ++i) best[size_t(parts[i].g)] = std::max(best[size_t(parts[i].g)], rhoG_[i]);
    std::vector<float> ax(drops.size(), 0.f), ay(drops.size(), 0.f);
    std::vector<int> an(drops.size(), 0);
    for (size_t i = 0; i < parts.size(); ++i) {
        const size_t g = size_t(parts[i].g);
        if (rhoG_[i] >= best[g] * 0.92f) {
            ax[g] += parts[i].x;
            ay[g] += parts[i].y;
            ++an[g];
        }
    }
    for (size_t g = 0; g < drops.size(); ++g)
        if (an[g]) {
            const float tx = ax[g] / float(an[g]), ty = std::min(ay[g] / float(an[g]), cfg::kFloor - 6.f);
            if (drops[g].lr == 0.f) {  // first time: snap
                drops[g].lx = tx;
                drops[g].ly = ty;
                drops[g].lr = 1.f;
            } else {
                drops[g].lx += (tx - drops[g].lx) * 0.4f;
                drops[g].ly += (ty - drops[g].ly) * 0.4f;
            }
        }
}

void World::animate(float dt) {
    clock_ += dt;
    (void)dt;
}

void World::substep(float h, float g, bool last) {
    static float restNear = 0.f;
    static const float rho0 = restDensity(restNear);
    const size_t n = parts.size();
    if (!n) return;

    // 1. gravity + viscosity (impulses between approaching neighbours)
    regeom();
    for (auto& p : parts) p.vy += g * h;
    for (const auto& pr : pairs_) {
        Particle &a = parts[size_t(pr.i)], &b = parts[size_t(pr.j)];
        const float u = (a.vx - b.vx) * pr.nx + (a.vy - b.vy) * pr.ny;  // >0 approaching
        if (u <= 0.f) continue;
        const float I = h * (1.f - pr.q) * (cfg::kViscSigma * u + cfg::kViscBeta * u * u) * 0.5f;
        a.vx -= I * pr.nx;
        a.vy -= I * pr.ny;
        b.vx += I * pr.nx;
        b.vy += I * pr.ny;
    }

    // 2. advect
    for (auto& p : parts) {
        const float sp = std::hypot(p.vx, p.vy);
        if (sp > cfg::kMaxSpeed) {
            p.vx *= cfg::kMaxSpeed / sp;
            p.vy *= cfg::kMaxSpeed / sp;
        }
        p.px = p.x;
        p.py = p.y;
        p.x += p.vx * h;
        p.y += p.vy * h;
    }

    // 3. double density relaxation
    regeom();
    rho_.assign(n, 0.f);
    rhoN_.assign(n, 0.f);
    for (const auto& pr : pairs_) {
        const float w = 1.f - pr.q;
        rho_[size_t(pr.i)] += w * w;
        rho_[size_t(pr.j)] += w * w;
        rhoN_[size_t(pr.i)] += w * w * w;
        rhoN_[size_t(pr.j)] += w * w * w;
    }
    const float h2 = h * h;
    auto press = [&](size_t i) {
        float P = cfg::kStiff * (rho_[i] - rho0);
        if (P < 0.f) P *= 0.35f;  // weak suction at the surface: liquid cohesion
        return P;
    };
    for (const auto& pr : pairs_) {
        const size_t i = size_t(pr.i), j = size_t(pr.j);
        const float w = 1.f - pr.q;
        const float D = h2 * ((press(i) + press(j)) * w + cfg::kStiffNear * (rhoN_[i] + rhoN_[j]) * w * w);
        const float hx = 0.5f * D * pr.nx, hy = 0.5f * D * pr.ny;
        parts[j].x += hx;
        parts[j].y += hy;
        parts[i].x -= hx;
        parts[i].y -= hy;
    }

    // 4. cohesion inside a drop keeps each colour together
    for (const auto& pr : pairs_) {
        Particle &a = parts[size_t(pr.i)], &b = parts[size_t(pr.j)];
        if (a.g != b.g) continue;
        const float d = pr.q * cfg::kInteract;
        if (d <= cfg::kSpacing * 0.95f) continue;
        const float pull = cfg::kCohesion * (1.f - pr.q) * 0.5f;
        a.x += pull * pr.nx;
        a.y += pull * pr.ny;
        b.x -= pull * pr.nx;
        b.y -= pull * pr.ny;
    }
    for (auto& p : parts) {
        const Drop& d = drops[size_t(p.g)];
        const float hold = cfg::kHold * std::max(0.f, 1.f - d.age / 0.7f);
        if (hold > 0.f) {
            p.x += (d.x - p.x) * hold;
            p.y += (d.y - p.y) * hold;
        }
    }

    // 5. container + velocity from position change
    for (auto& p : parts) {
        bool wall = false;
        const float lo = cfg::kFieldL + 1.2f, hi = cfg::kFieldR - 1.2f, fl = cfg::kFloor - 1.2f;
        if (p.x < lo) { p.x = lo; wall = true; }
        if (p.x > hi) { p.x = hi; wall = true; }
        if (p.y > fl) { p.y = fl; wall = true; }
        p.vx = (p.x - p.px) / h;
        p.vy = (p.y - p.py) / h;
        if (wall) {
            p.vx *= cfg::kWallDrag;
            p.vy *= 0.9f;
        }
        if (p.vx * p.vx + p.vy * p.vy < 900.f) {  // slow liquid settles instead of jittering
            p.vx *= 0.9f;
            p.vy *= 0.9f;
        }
    }

    // 6. merge pass (once per fixed step): touching same-level drops fuse.
    if (!last) return;
    struct Contact { int a, b, c; };
    std::vector<Contact> contacts;
    regeom();
    for (const auto& pr : pairs_) {
        const int ga = parts[size_t(pr.i)].g, gb = parts[size_t(pr.j)].g;
        if (ga == gb || drops[size_t(ga)].level != drops[size_t(gb)].level || drops[size_t(ga)].level >= cfg::kMaxLevel) continue;
        if (pr.q * cfg::kInteract > cfg::kSpacing * 2.0f) continue;
        const int lo = std::min(ga, gb), hi = std::max(ga, gb);
        bool found = false;
        for (auto& c : contacts)
            if (c.a == lo && c.b == hi) { ++c.c; found = true; break; }
        if (!found) contacts.push_back({lo, hi, 1});
    }
    std::vector<int> used;
    // Merge from the highest index down so earlier indices stay valid.
    std::sort(contacts.begin(), contacts.end(), [](const Contact& x, const Contact& y) { return x.b > y.b; });
    for (const auto& c : contacts) {
        if (c.c < 2) continue;
        if (std::find(used.begin(), used.end(), c.a) != used.end() || std::find(used.begin(), used.end(), c.b) != used.end()) continue;
        used.push_back(c.a);
        used.push_back(c.b);
        mergeGroups(c.a, c.b);
    }
}

void World::mergeGroups(int a, int b) {
    Drop& A = drops[size_t(a)];
    const int level = A.level + 1;
    for (auto& p : parts)
        if (p.g == b) p.g = a;
    // Volume per level is fixed, so shed the surplus particles evenly.
    const int target = particlesFor(level);
    int have = 0;
    for (const auto& p : parts) have += p.g == a;
    int drop = std::max(0, have - target);
    float acc = 0.f;
    const float rate = have ? float(drop) / float(have) : 0.f;
    float cx = 0, cy = 0;
    for (const auto& p : parts)
        if (p.g == a) { cx += p.x; cy += p.y; }
    cx /= float(std::max(have, 1));
    cy /= float(std::max(have, 1));
    for (auto& p : parts) {
        if (p.g != a) continue;
        acc += rate;
        if (acc >= 1.f && drop > 0) {
            acc -= 1.f;
            --drop;
            p.g = -1;
            continue;
        }
        // Splash: a short outward + upward kick so the new bigger drop surges.
        float ex = p.x - cx, ey = p.y - cy;
        const float d = std::max(1.f, std::hypot(ex, ey));
        p.vx += ex / d * 45.f;
        p.vy += ey / d * 45.f - 55.f;
    }
    parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Particle& p) { return p.g < 0; }), parts.end());
    candValid_ = false;
    A.level = level;
    A.age = 0.8f;  // grace before it can count as overflow
    drops[size_t(b)].dead = true;
    removeDrop(b);
    const int na = a > b ? a - 1 : a;
    chain_ = sinceMerge_ < cfg::kChainWindow ? chain_ + 1 : 1;
    sinceMerge_ = 0.f;
    refresh();
    events.push_back({drops[size_t(na)].x, drops[size_t(na)].y, level, chain_});
}

void World::removeDrop(int gi) {
    drops.erase(drops.begin() + gi);
    for (auto& p : parts)
        if (p.g > gi) --p.g;
}

void World::updateOverflow(float deadY, float dt) {
    bool over = false;
    std::vector<int> cnt(drops.size(), 0);
    for (const auto& p : parts)
        if (p.y < deadY) ++cnt[size_t(p.g)];
    for (size_t i = 0; i < drops.size(); ++i)
        if (cnt[i] >= 6 && drops[i].age > 1.2f && std::hypot(drops[i].vx, drops[i].vy) < 45.f) over = true;
    overflow_ = over ? overflow_ + dt : std::max(0.f, overflow_ - dt * 2.f);
}

std::vector<Drop> World::popAbove(float deadY) {
    std::vector<Drop> out;
    std::vector<int> cnt(drops.size(), 0);
    for (const auto& p : parts)
        if (p.y < deadY) ++cnt[size_t(p.g)];
    for (int i = int(drops.size()) - 1; i >= 0; --i)
        if (cnt[size_t(i)] >= 6 && drops[size_t(i)].age > 1.2f) {
            out.push_back(drops[size_t(i)]);
            for (auto& p : parts)
                if (p.g == i) p.g = -1;
            parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Particle& p) { return p.g < 0; }), parts.end());
            candValid_ = false;
            removeDrop(i);
        }
    refresh();
    return out;
}

float World::surfaceY(float x, float hw) const {
    float y = cfg::kFloor;
    for (const auto& p : parts)
        if (std::fabs(p.x - x) <= hw) y = std::min(y, p.y);
    return y;
}

void World::particleSprites(float alpha, float radius, std::vector<BlobSprite>& out) const {
    out.reserve(out.size() + parts.size());
    for (const auto& p : parts) {
        BlobSprite s;
        s.x = p.ox + (p.x - p.ox) * alpha;
        s.y = p.oy + (p.y - p.oy) * alpha;
        s.ra = s.rb = radius;
        s.level = drops[size_t(p.g)].level;
        out.push_back(s);
    }
}

void World::labelSprites(std::vector<BlobSprite>& out) const {
    for (const auto& d : drops) {
        if (!d.n) continue;
        BlobSprite s;
        s.x = d.lx;
        s.y = d.ly;
        s.level = d.level;
        const float tr = 0.5f * (d.sxx + d.syy), df = 0.5f * (d.sxx - d.syy);
        const float disc = std::sqrt(df * df + d.sxy * d.sxy);
        const float ra = 2.f * std::sqrt(std::max(0.f, tr + disc)), rb = 2.f * std::sqrt(std::max(0.f, tr - disc));
        // Flat layers are wide but thin: size the number by thickness, with some allowance for width.
        s.ra = s.rb = std::min(std::sqrt(ra * rb), rb * 1.8f);
        out.push_back(s);
    }
}

}  // namespace liq
