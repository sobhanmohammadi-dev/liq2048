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
    impacts.clear();
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
            p.rx = px - x;
            p.ry = py - y;
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

void World::picture(std::vector<int>& levels, std::vector<int>& counts, std::vector<uint16_t>& pos) const {
    levels.clear();
    counts.clear();
    pos.clear();
    std::vector<std::vector<int>> byDrop(drops.size());
    for (size_t i = 0; i < parts.size(); ++i) byDrop[size_t(parts[i].g)].push_back(int(i));
    auto q = [](float v) { return uint16_t(std::min(65535.f, std::max(0.f, v * 128.f + 0.5f))); };
    for (size_t g = 0; g < drops.size(); ++g) {
        if (byDrop[g].empty()) continue;
        levels.push_back(drops[g].level);
        counts.push_back(int(byDrop[g].size()));
        for (int i : byDrop[g]) {
            pos.push_back(q(parts[size_t(i)].x));
            pos.push_back(q(parts[size_t(i)].y));
        }
    }
}

void World::setPicture(const std::vector<int>& levels, const std::vector<int>& counts, const std::vector<uint16_t>& pos) {
    // Same drops as before: glide from the old positions (and keep the number labels where they were).
    bool same = levels.size() == drops.size();
    size_t total = 0;
    for (size_t g = 0; g < levels.size() && same; ++g) same = drops[g].level == levels[g] && drops[g].n == counts[g];
    for (int n : counts) total += size_t(n);
    same = same && total == parts.size();
    std::vector<Drop> oldDrops = drops;
    std::vector<Particle> old = parts;
    parts.assign(total, Particle{});
    drops.assign(levels.size(), Drop{});
    size_t k = 0;
    for (size_t g = 0; g < levels.size(); ++g) {
        drops[g].level = levels[g];
        drops[g].age = 5.f;
        if (same) {
            drops[g].lx = oldDrops[g].lx;
            drops[g].ly = oldDrops[g].ly;
            drops[g].lr = oldDrops[g].lr;
        }
        for (int j = 0; j < counts[g]; ++j, ++k) {
            Particle& p = parts[k];
            p.x = float(pos[k * 2]) / 128.f;
            p.y = float(pos[k * 2 + 1]) / 128.f;
            p.g = int(g);
            p.ox = same ? old[k].x : p.x;
            p.oy = same ? old[k].y : p.y;
        }
    }
    candValid_ = false;
    refresh();
    anchors();
}

// Positions only, packed: the pair loops read two random particles per pair, and a 8-byte-per-particle array stays in
// the L1 cache where the full particle records (48 bytes) do not.
void World::gatherPositions() {
    pos_.resize(parts.size());
    for (size_t i = 0; i < parts.size(); ++i) pos_[i] = {parts[i].x, parts[i].y};
}

void World::buildPairs(float margin) {
    const float R = cfg::kInteract + margin;
    const float cell = R * 0.5f;
    const int nx = int(worldW / cell) + 5, ny = int(worldH / cell) + 5;
    const size_t n = parts.size(), cells = size_t(nx) * size_t(ny);
    gatherPositions();

    // Counting sort of the particles into cells: one contiguous run per cell instead of a linked list, so the pair search
    // walks memory in order. Inside a cell the particles are stored by DESCENDING index, which is exactly the order the
    // old linked list had, so the candidate pairs (and with them every float sum of the solver) come out unchanged.
    cellStart_.assign(cells + 1, 0);
    cellOf_.resize(n);
    int minCx = nx, maxCx = 0, minCy = ny, maxCy = 0;
    for (size_t i = 0; i < n; ++i) {
        const int cx = std::clamp(int(pos_[i].x / cell) + 2, 0, nx - 1), cy = std::clamp(int(pos_[i].y / cell) + 2, 0, ny - 1);
        minCx = std::min(minCx, cx);
        maxCx = std::max(maxCx, cx);
        minCy = std::min(minCy, cy);
        maxCy = std::max(maxCy, cy);
        const int id = cy * nx + cx;
        cellOf_[i] = id;
        ++cellStart_[size_t(id) + 1];
    }
    for (size_t c = 0; c < cells; ++c) cellStart_[c + 1] += cellStart_[c];
    fill_.assign(cellStart_.begin(), cellStart_.end() - 1);
    sorted_.resize(n);
    sortedPos_.resize(n);
    for (size_t i = n; i-- > 0;) {
        const size_t at = size_t(fill_[size_t(cellOf_[i])]++);
        sorted_[at] = int(i);
        sortedPos_[at] = pos_[i];
    }

    cand_.clear();
    const float R2 = R * R;
    // Half-shell of neighbour cells within +-2.
    static const int off[13][2] = {{0, 0}, {1, 0}, {2, 0}, {-2, 1}, {-1, 1}, {0, 1}, {1, 1}, {2, 1},
                                   {-2, 2}, {-1, 2}, {0, 2}, {1, 2}, {2, 2}};
    for (int cy = minCy; cy <= maxCy; ++cy)
        for (int cx = minCx; cx <= maxCx; ++cx) {
            const int s0 = cellStart_[size_t(cy * nx + cx)], e0 = cellStart_[size_t(cy * nx + cx) + 1];
            if (s0 == e0) continue;
            for (int o = 0; o < 13; ++o) {
                const int ax = cx + off[o][0], ay = cy + off[o][1];
                if (ax < 0 || ax >= nx || ay >= ny) continue;
                const int nid = ay * nx + ax;
                const int s1 = cellStart_[size_t(nid)], e1 = cellStart_[size_t(nid) + 1];
                for (int ia = s0; ia < e0; ++ia) {
                    const Pos a = sortedPos_[size_t(ia)];
                    for (int ib = o == 0 ? ia + 1 : s1, end = o == 0 ? e0 : e1; ib < end; ++ib) {
                        const float dx = sortedPos_[size_t(ib)].x - a.x, dy = sortedPos_[size_t(ib)].y - a.y;
                        if (dx * dx + dy * dy >= R2) continue;
                        cand_.push_back({sorted_[size_t(ia)], sorted_[size_t(ib)], 0.f, dx >= 0.f ? 1.f : -1.f, 0.f});  // geometry: regeom()
                    }
                }
            }
        }
    candValid_ = true;
}

// Recompute pair geometry after particles moved; drop pairs that left the interaction radius.
void World::regeom() {
    gatherPositions();
    const float R2 = cfg::kInteract * cfg::kInteract;
    pairs_.resize(cand_.size());
    PairRec* out = pairs_.data();
    size_t kept = 0;
    for (const PairRec& c : cand_) {
        const Pos &a = pos_[size_t(c.i)], &b = pos_[size_t(c.j)];
        const float dx = b.x - a.x, dy = b.y - a.y, d2 = dx * dx + dy * dy;
        // Branch-free filter: every candidate is written, only the ones inside the radius advance the cursor
        // (whether a pair is inside is unpredictable, so a branch here costs a pipeline flush about every other pair).
        PairRec& pr = out[kept];
        pr = c;
        float d = std::sqrt(d2);
        if (d < 1e-4f) {  // exactly stacked: separate along a fixed axis
            pr.nx = (c.i + c.j) & 1 ? 1.f : -0.6f;
            pr.ny = (c.i * 7 + c.j) & 1 ? 0.8f : -0.8f;
            const float l = std::hypot(pr.nx, pr.ny);
            pr.nx /= l;
            pr.ny /= l;
            d = 1e-4f;
        } else {
            const float inv = 1.f / d;
            pr.nx = dx * inv;
            pr.ny = dy * inv;
        }
        pr.q = d / cfg::kInteract;
        kept += d2 < R2;
    }
    pairs_.resize(kept);
}

void World::step(float dt, float gravity, float gx) {
    gx_ = gx;
    for (auto& p : parts) {
        p.ox = p.x;
        p.oy = p.y;
    }
    const int ns = std::clamp(substeps, 1, 3);
    const float h = dt / float(ns);
    buildPairs(3.f);  // candidate pairs with a margin, reused by every substep
    for (int s = 0; s < ns; ++s) substep(h, gravity, s == ns - 1);
    for (auto& d : drops) d.age += dt;
    refresh();
    for (auto& d : drops) {
        // A fast-falling drop that suddenly stops has hit the floor or the pile.
        if (d.pvy > 110.f && d.vy < d.pvy * 0.5f && d.age > 0.05f) {
            const float size = 0.35f + 0.65f * std::min(1.f, float(d.level) / 8.f);
            impacts.push_back({d.x, d.y, std::min(1.f, d.pvy / 300.f) * size, d.level, 0u});
        }
        d.pvy = d.vy;
    }
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
            const float tx = ax[g] / float(an[g]), ty = std::min(ay[g] / float(an[g]), floorY - 6.f);
            if (drops[g].lr == 0.f) {  // first time: snap
                drops[g].lx = tx;
                drops[g].ly = ty;
                drops[g].lr = 1.f;
            } else {
                // Follow quickly while the drop falls or splashes; once it rests, ignore small wobble (dead zone)
                // and glide slowly, so the numbers stay put instead of jittering with the liquid.
                Drop& d = drops[g];
                const bool resting = d.age > 1.2f && d.vx * d.vx + d.vy * d.vy < 35.f * 35.f;
                const float ex = tx - d.lx, ey = ty - d.ly;
                const float dist = std::sqrt(ex * ex + ey * ey);
                float k = 0.4f;
                if (resting) k = dist < 2.5f ? 0.f : std::min(0.12f, 0.02f + (dist - 2.5f) * 0.01f);
                d.lx += ex * k;
                d.ly += ey * k;
            }
        }
}

void World::substep(float h, float g, bool last) {
    static float restNear = 0.f;
    static const float rho0 = restDensity(restNear);
    const size_t n = parts.size();
    if (!n) return;

    // 1. gravity + viscosity (impulses between approaching neighbours)
    regeom();
    for (auto& p : parts) {
        p.vy += g * h;
        p.vx += gx_ * h;
    }
    for (const auto& pr : pairs_) {
        Particle &a = parts[size_t(pr.i)], &b = parts[size_t(pr.j)];
        if (shear > 0.f) {  // neighbours drag each other towards a common velocity (gooey liquids)
            const float k = shear * (1.f - pr.q), dvx = (b.vx - a.vx) * k, dvy = (b.vy - a.vy) * k;
            a.vx += dvx;
            a.vy += dvy;
            b.vx -= dvx;
            b.vy -= dvy;
        }
        const float u = (a.vx - b.vx) * pr.nx + (a.vy - b.vy) * pr.ny;  // >0 approaching
        if (u <= 0.f) continue;
        const float I = h * (1.f - pr.q) * (cfg::kViscSigma * viscScale * u + cfg::kViscBeta * u * u) * 0.5f;
        a.vx -= I * pr.nx;
        a.vy -= I * pr.ny;
        b.vx += I * pr.nx;
        b.vy += I * pr.ny;
    }

    // 1b. jelly: a spring pulls every particle to its rest place around the drop's centroid, and the motion relative to
    // the drop's mean velocity is damped, so a drop wobbles and springs back to a round body (shape memory).
    if (jelly > 0.f) {
        jc_.assign(drops.size() * 5, 0.f);
        for (const auto& p : parts) {
            float* c = &jc_[size_t(p.g) * 5];
            c[0] += p.x;
            c[1] += p.y;
            c[2] += p.vx;
            c[3] += p.vy;
            c[4] += 1.f;
        }
        const float kd = jellyDamp * h;
        for (auto& p : parts) {
            const float* c = &jc_[size_t(p.g) * 5];
            const float inv = 1.f / c[4];
            p.vx += (c[0] * inv + p.rx - p.x) * jelly * h - (p.vx - c[2] * inv) * kd;
            p.vy += (c[1] * inv + p.ry - p.y) * jelly * h - (p.vy - c[3] * inv) * kd;
        }
    }

    // 2. advect
    for (auto& p : parts) {
        const float sp2 = p.vx * p.vx + p.vy * p.vy;
        if (sp2 > cfg::kMaxSpeed * cfg::kMaxSpeed) {
            const float k = cfg::kMaxSpeed / std::sqrt(sp2);
            p.vx *= k;
            p.vy *= k;
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
    press_.resize(n);
    for (size_t i = 0; i < n; ++i) {
        float P = cfg::kStiff * (rho_[i] - rho0);
        if (P < 0.f) P *= 0.35f;  // weak suction at the surface: liquid cohesion
        press_[i] = P;
    }
    for (const auto& pr : pairs_) {
        const size_t i = size_t(pr.i), j = size_t(pr.j);
        const float w = 1.f - pr.q;
        const float D = h2 * ((press_[i] + press_[j]) * w + cfg::kStiffNear * (rhoN_[i] + rhoN_[j]) * w * w);
        const float hx = 0.5f * D * pr.nx, hy = 0.5f * D * pr.ny;
        parts[j].x += hx;
        parts[j].y += hy;
        parts[i].x -= hx;
        parts[i].y -= hy;
    }

    // 4. surface tension keeps a falling or splashing drop together. A drop at rest has none: the liquid then
    // flows and levels like water (a flat surface, waves that travel and fade) instead of staying in lumps.
    moving_.resize(drops.size());
    for (size_t g2 = 0; g2 < drops.size(); ++g2) {
        const Drop& dr = drops[g2];
        moving_[g2] = dr.age < 1.2f || dr.vx * dr.vx + dr.vy * dr.vy > 35.f * 35.f;
    }
    for (const auto& pr : pairs_) {
        Particle &a = parts[size_t(pr.i)], &b = parts[size_t(pr.j)];
        if (a.g != b.g) continue;
        const float d = pr.q * cfg::kInteract;
        if (d <= cfg::kSpacing) continue;
        // Surface tension acts while the drop is in motion; a sticky liquid keeps some of it at rest (restCohesion).
        const float k = moving_[size_t(a.g)] ? cfg::kCohesion * cohesion : cfg::kCohesion * restCohesion;
        if (k <= 0.f) continue;
        const float pull = k * (1.f - pr.q) * 0.5f;
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
        const float lo = fieldL + 1.2f, hi = fieldR - 1.2f, fl = floorY - 1.2f;
        if (p.x < lo) { p.x = lo; wall = true; }
        if (p.x > hi) { p.x = hi; wall = true; }
        if (p.y > fl) { p.y = fl; wall = true; }
        p.vx = (p.x - p.px) / h;
        p.vy = (p.y - p.py) / h;
        if (wall) {
            p.vx *= wallDrag;
            p.vy *= wallVy;
        }
        p.vx *= damp;
        p.vy *= damp;
        const float sp2 = p.vx * p.vx + p.vy * p.vy;  // position corrections can spike: keep the velocity sane
        if (sp2 > cfg::kHardSpeed * cfg::kHardSpeed) {
            const float k = cfg::kHardSpeed / std::sqrt(sp2);
            p.vx *= k;
            p.vy *= k;
        }
        if (p.vx * p.vx + p.vy * p.vy < cfg::kSleepV * cfg::kSleepV) {  // slow liquid settles instead of jittering
            p.vx *= cfg::kSleepF;
            p.vy *= cfg::kSleepF;
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
        if (pr.q * cfg::kInteract > cfg::kSpacing * 2.6f) continue;
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
    if (jelly > 0.f) {  // the merged drop remembers a round body of its new size (particles keep their direction, ranked by distance)
        const Drop& D = drops[size_t(na)];
        std::vector<std::pair<float, size_t>> order;
        for (size_t k = 0; k < parts.size(); ++k)
            if (parts[k].g == na) order.push_back({std::hypot(parts[k].x - D.x, parts[k].y - D.y), k});
        std::sort(order.begin(), order.end());
        const float R = radiusFor(level) - 0.5f * cfg::kSpacing;
        for (size_t r = 0; r < order.size(); ++r) {
            Particle& p = parts[order[r].second];
            const float dx = p.x - D.x, dy = p.y - D.y, d = std::max(0.01f, order[r].first);
            const float tr = R * std::sqrt((float(r) + 0.5f) / float(order.size()));
            p.rx = d < 0.5f ? 0.f : dx / d * tr;
            p.ry = d < 0.5f ? 0.f : dy / d * tr;
        }
    }
    events.push_back({drops[size_t(na)].x, drops[size_t(na)].y, level, chain_});
}

void World::relevel(int i, int level) {
    if (i < 0 || i >= int(drops.size())) return;
    level = std::clamp(level, 0, cfg::kMaxLevel);
    if (drops[size_t(i)].level == level) return;
    // Keep the centroid and the mean velocity; the new body is a fresh lattice of the right size.
    const float x = drops[size_t(i)].x, vx = drops[size_t(i)].vx, vy = drops[size_t(i)].vy;
    const float r = radiusFor(level);
    float y = drops[size_t(i)].y;
    y = std::min(y, floorY - r - 1.f);  // never inside the floor
    const float cx = std::clamp(x, fieldL + r + 1.f, fieldR - r - 1.f);
    parts.erase(std::remove_if(parts.begin(), parts.end(), [i](const Particle& p) { return p.g == i; }), parts.end());
    const size_t before = parts.size();
    spawn(cx, y, level);  // appended as the last drop
    const int last = int(drops.size()) - 1;
    for (size_t k = before; k < parts.size(); ++k) {
        parts[k].vx = vx;
        parts[k].vy = vy;
    }
    // Move the new drop into slot i so the other drops keep their indices.
    Drop nd = drops[size_t(last)];
    drops.pop_back();
    for (auto& p : parts)
        if (p.g == last) p.g = i;
    nd.age = 0.9f;  // no overflow grace needed, but keep the pop spring calm
    nd.pop = 0.f;
    drops[size_t(i)] = nd;
    candValid_ = false;
    refresh();
    anchors();
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
    float y = floorY;
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
