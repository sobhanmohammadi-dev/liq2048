#include "game/Liquid.hpp"
#include "game/Modifier.hpp"
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
    if (r.substeps < 1 || r.substeps > 3 || r.modifier < 0 || r.modifier >= kModifierCount) return false;
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

static const char* kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static uint16_t crc16(const std::vector<uint8_t>& b) {
    uint16_t c = 0xFFFF;
    for (uint8_t v : b) {
        c ^= uint16_t(v) << 8;
        for (int i = 0; i < 8; ++i) c = (c & 0x8000) ? uint16_t((c << 1) ^ 0x1021) : uint16_t(c << 1);
    }
    return c;
}

static void putVar(std::vector<uint8_t>& b, uint32_t v) {
    while (v >= 0x80) {
        b.push_back(uint8_t(v | 0x80));
        v >>= 7;
    }
    b.push_back(uint8_t(v));
}

static bool getVar(const std::vector<uint8_t>& b, size_t& i, uint32_t& out) {
    uint32_t v = 0;
    int shift = 0;
    for (;;) {
        if (i >= b.size() || shift > 28) return false;
        const uint8_t x = b[i++];
        v |= uint32_t(x & 0x7F) << shift;
        if (!(x & 0x80)) break;
        shift += 7;
    }
    out = v;
    return true;
}

std::string Replay::toCode() const {
    std::vector<uint8_t> b;
    auto u32 = [&](uint32_t v) {
        for (int i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (8 * i)));
    };
    b.push_back(uint8_t(mode));
    b.push_back(uint8_t(modifier * 4 + (substeps - 1)));
    b.push_back(uint8_t(puzzle));
    u32(seed);
    uint32_t gb;
    std::memcpy(&gb, &gravity, 4);
    u32(gb);
    b.push_back(uint8_t(physics));
    b.push_back(uint8_t(outcome | liquid << 2));  // the liquid type rides in the upper bits (old codes: Water)
    putVar(b, endStep);
    putVar(b, uint32_t(subs.size()));
    uint32_t ps = 0;
    for (const auto& sw : subs) {
        putVar(b, sw.first - ps);
        ps = sw.first;
        b.push_back(sw.second);
    }
    uint32_t prev = 0;
    for (const auto& d : drops) {
        putVar(b, d.step - prev);
        prev = d.step;
        const uint16_t q = uint16_t(int(d.x * 128.f + 0.5f));
        b.push_back(uint8_t(q & 255));
        b.push_back(uint8_t(q >> 8));
        b.push_back(uint8_t(d.level));
    }
    const uint16_t crc = crc16(b);
    b.push_back(uint8_t(crc & 255));
    b.push_back(uint8_t(crc >> 8));
    std::string s = "LQ2-";
    for (size_t i = 0; i < b.size(); i += 3) {
        uint32_t v = uint32_t(b[i]) << 16 | (i + 1 < b.size() ? uint32_t(b[i + 1]) << 8 : 0u) | (i + 2 < b.size() ? b[i + 2] : 0u);
        s += kB64[(v >> 18) & 63];
        s += kB64[(v >> 12) & 63];
        if (i + 1 < b.size()) s += kB64[(v >> 6) & 63];
        if (i + 2 < b.size()) s += kB64[v & 63];
    }
    return s;
}

bool Replay::fromCode(const std::string& in, Replay& out) {
    size_t p = in.find("LQ2-");
    int ver = 2;
    if (p == std::string::npos) {
        p = in.find("LQ1-");
        ver = 1;
    }
    if (p == std::string::npos) return false;
    std::vector<uint8_t> b;
    uint32_t acc = 0;
    int bits = 0;
    for (size_t i = p + 4; i < in.size(); ++i) {
        const char ch = in[i];
        if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') {
            if (b.empty() && bits == 0) continue;
            break;
        }
        const char* f = std::strchr(kB64, ch);
        if (!f || !ch) return false;
        acc = (acc << 6) | uint32_t(f - kB64);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            b.push_back(uint8_t(acc >> bits));
            acc &= (1u << bits) - 1u;
        }
        if (b.size() > 400000) return false;
    }
    if (b.size() < 13 + 2) return false;
    const uint16_t crc = uint16_t(b[b.size() - 2] | b[b.size() - 1] << 8);
    b.resize(b.size() - 2);
    if (crc16(b) != crc) return false;
    Replay r;
    size_t i = 0;
    r.mode = b[i++];
    const int ms = b[i++];
    r.modifier = ms / 4;
    r.substeps = ms % 4 + 1;
    r.puzzle = b[i++];
    auto u32 = [&]() {
        uint32_t v = 0;
        for (int k = 0; k < 4; ++k) v |= uint32_t(b[i++]) << (8 * k);
        return v;
    };
    r.seed = u32();
    const uint32_t gb = u32();
    std::memcpy(&r.gravity, &gb, 4);
    if (r.substeps > 3 || r.modifier >= kModifierCount || r.mode > 15 || !(r.gravity >= 0.f && r.gravity < 100000.f)) return false;
    r.physics = 1;
    if (ver == 2) {
        if (i + 2 > b.size()) return false;
        r.physics = b[i++];
        r.outcome = b[i] & 3;
        r.liquid = b[i++] >> 2;
        uint32_t n = 0;
        if (r.liquid >= kLiquidCount || !getVar(b, i, r.endStep) || !getVar(b, i, n) || n > 100000) return false;
        uint32_t st = 0;
        for (uint32_t k = 0; k < n; ++k) {
            uint32_t d = 0;
            if (!getVar(b, i, d) || i >= b.size()) return false;
            st += d;
            const uint8_t v = b[i++];
            if (v < 1 || v > 3) return false;
            r.subs.push_back({st, v});
        }
    }
    uint32_t step = 0;
    while (i < b.size()) {
        uint32_t delta = 0;
        if (!getVar(b, i, delta)) return false;
        if (i + 3 > b.size()) return false;
        step += delta;
        ReplayDrop d;
        d.step = step;
        d.x = float(b[i] | b[i + 1] << 8) / 128.f;
        d.level = b[i + 2];
        i += 3;
        if (d.level > 11) return false;
        r.drops.push_back(d);
    }
    out = std::move(r);
    return true;
}

uint64_t replayChecksum(const Replay& r, int tailSteps) {
    World w;
    w.substeps = r.substeps;
    applyLiquid(w, r.liquid, r.modifier == kModThick);
    size_t i = 0, si = 0;
    uint32_t step = 0;
    const uint32_t end = (r.drops.empty() ? 0u : r.drops.back().step) + uint32_t(tailSteps);
    for (; step < end; ++step) {
        while (si < r.subs.size() && r.subs[si].first <= step) w.substeps = r.subs[si++].second;
        while (i < r.drops.size() && r.drops[i].step <= step) {
            w.spawn(r.drops[i].x, cfg::kSpawnY, r.drops[i].level);
            ++i;
        }
        w.step(cfg::kStep, r.gravity, modifierSideways(r.modifier, uint32_t(step)));
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
