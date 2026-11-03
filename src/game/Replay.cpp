#include "game/Replay.hpp"

#include <cstdio>
#include <cstring>
#include <sstream>

#include "core/Config.hpp"
#include "game/World.hpp"

namespace liq {

void Replay::truncateFrom(uint32_t step) {
    while (!drops.empty() && drops.back().step >= step) drops.pop_back();
}

std::string Replay::toText() const {
    std::ostringstream o;
    o << "LIQREPLAY 1\n"
      << mode << ' ' << modifier << ' ' << substeps << ' ' << seed << ' ' << puzzle << ' ';
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.9g", double(gravity));
    o << buf << '\n';
    for (const auto& d : drops) {
        std::snprintf(buf, sizeof buf, "%.9g", double(d.x));
        o << d.step << ' ' << buf << ' ' << d.level << '\n';
    }
    return o.str();
}

bool Replay::fromText(const std::string& s, Replay& out) {
    std::istringstream in(s);
    std::string magic;
    int ver = 0;
    if (!(in >> magic >> ver) || magic != "LIQREPLAY" || ver != 1) return false;
    Replay r;
    double g = 0;
    if (!(in >> r.mode >> r.modifier >> r.substeps >> r.seed >> r.puzzle >> g)) return false;
    if (r.substeps < 1 || r.substeps > 3 || r.modifier < 0 || r.modifier > 4) return false;
    r.gravity = float(g);
    ReplayDrop d;
    double x;
    uint32_t last = 0;
    while (in >> d.step >> x >> d.level) {
        if (d.step < last || d.level < 0 || d.level > 11) return false;
        last = d.step;
        d.x = float(x);
        r.drops.push_back(d);
        if (r.drops.size() > 100000) return false;
    }
    out = std::move(r);
    return true;
}

uint64_t replayChecksum(const Replay& r, int tailSteps) {
    World w;
    w.substeps = r.substeps;
    w.viscScale = r.modifier == 3 ? 3.f : 1.f;
    size_t i = 0;
    uint32_t step = 0;
    const uint32_t end = (r.drops.empty() ? 0u : r.drops.back().step) + uint32_t(tailSteps);
    for (; step < end; ++step) {
        while (i < r.drops.size() && r.drops[i].step <= step) {
            w.spawn(r.drops[i].x, cfg::kSpawnY, r.drops[i].level);
            ++i;
        }
        w.step(cfg::kStep, r.gravity);
        w.events.clear();
        w.impacts.clear();
    }
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](float f) {
        uint32_t u;
        std::memcpy(&u, &f, 4);
        h = (h ^ u) * 1099511628211ull;
    };
    for (const auto& p : w.parts) {
        mix(p.x);
        mix(p.y);
    }
    h = (h ^ uint64_t(w.parts.size())) * 1099511628211ull;
    return h;
}

}  // namespace liq
