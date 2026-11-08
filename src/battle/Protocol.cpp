#include "battle/Protocol.hpp"

#include <algorithm>

namespace liq::battle {

namespace {

struct W {
    net::Bytes b;
    void u8(uint32_t v) { b.push_back(uint8_t(v)); }
    void u16(uint32_t v) { u8(v & 255), u8(v >> 8); }
    void u32(uint32_t v) { u16(v & 0xFFFF), u16(v >> 16); }
    void str(const std::string& s) {
        const size_t n = std::min<size_t>(s.size(), 24);
        u8(uint32_t(n));
        b.insert(b.end(), s.begin(), s.begin() + long(n));
    }
};

struct R {
    const net::Bytes& b;
    size_t i = 0;
    bool ok = true;
    explicit R(const net::Bytes& bytes) : b(bytes) {}
    uint32_t u8() {
        if (i >= b.size()) {
            ok = false;
            return 0;
        }
        return b[i++];
    }
    uint32_t u16() {
        const uint32_t lo = u8();
        return lo | u8() << 8;
    }
    uint32_t u32() {
        const uint32_t lo = u16();
        return lo | u16() << 16;
    }
    std::string str() {
        const size_t n = u8();
        if (i + n > b.size()) {
            ok = false;
            return {};
        }
        std::string s(b.begin() + long(i), b.begin() + long(i + n));
        i += n;
        for (auto& c : s)
            if (static_cast<unsigned char>(c) < 32) c = '?';
        return s;
    }
};

}  // namespace

net::Bytes encode(const Hello& m) {
    W w;
    w.u8(uint32_t(Msg::Hello));
    w.u8(kProtocol);
    w.str(m.name);
    return w.b;
}
net::Bytes encode(const Welcome& m) {
    W w;
    w.u8(uint32_t(Msg::Welcome));
    w.u8(kProtocol);
    w.u32(m.seed);
    w.u8(m.modifier);
    w.u16(m.duration);
    w.str(m.hostName);
    return w.b;
}
net::Bytes encodeStart() { return {uint8_t(Msg::Start)}; }
net::Bytes encodeLeave() { return {uint8_t(Msg::Leave)}; }
net::Bytes encode(const Final& m) {
    W w;
    w.u8(uint32_t(Msg::Final));
    w.u32(uint32_t(m.score));
    w.u8(m.maxLevel);
    w.u8(m.toppedOut);
    return w.b;
}
net::Bytes encode(const Frame& m) {
    W w;
    w.u8(uint32_t(Msg::Frame));
    w.u32(uint32_t(m.score));
    w.u8(m.maxLevel);
    w.u16(m.drops);
    w.u8(m.flags);
    w.u16(m.snap.aimX);
    w.u8(m.snap.heldLevel);
    w.u16(uint32_t(m.snap.drops.size()));
    for (const auto& d : m.snap.drops) {
        w.u8(d.level);
        w.u16(d.n);
    }
    for (uint16_t v : m.snap.pos) w.u16(v);
    return w.b;
}

bool decode(const net::Bytes& b, Hello& m) {
    R r(b);
    if (r.u8() != uint32_t(Msg::Hello) || r.u8() != kProtocol) return false;
    m.name = r.str();
    return r.ok;
}
bool decode(const net::Bytes& b, Welcome& m) {
    R r(b);
    if (r.u8() != uint32_t(Msg::Welcome) || r.u8() != kProtocol) return false;
    m.seed = r.u32();
    m.modifier = uint8_t(r.u8());
    m.duration = uint16_t(r.u16());
    m.hostName = r.str();
    return r.ok && m.modifier <= 6 && m.duration >= 10 && m.duration <= 900;
}
bool decode(const net::Bytes& b, Final& m) {
    R r(b);
    if (r.u8() != uint32_t(Msg::Final)) return false;
    m.score = int32_t(r.u32());
    m.maxLevel = uint8_t(r.u8());
    m.toppedOut = uint8_t(r.u8());
    return r.ok;
}
bool decode(const net::Bytes& b, Frame& m) {
    R r(b);
    if (r.u8() != uint32_t(Msg::Frame)) return false;
    m.score = int32_t(r.u32());
    m.maxLevel = uint8_t(r.u8());
    m.drops = uint16_t(r.u16());
    m.flags = uint8_t(r.u8());
    m.snap.aimX = uint16_t(r.u16());
    m.snap.heldLevel = uint8_t(r.u8());
    const uint32_t nd = r.u16();
    m.snap.drops.clear();
    m.snap.pos.clear();
    size_t total = 0;
    for (uint32_t i = 0; i < nd && r.ok; ++i) {
        SnapDrop d;
        d.level = uint8_t(r.u8());
        d.n = uint16_t(r.u16());
        if (d.level > 11) return false;
        total += d.n;
        if (total > 6000) return false;
        m.snap.drops.push_back(d);
    }
    if (!r.ok || r.i + total * 4 != b.size()) return false;  // exact size: anything else is damaged
    m.snap.pos.reserve(total * 2);
    for (size_t i = 0; i < total * 2; ++i) m.snap.pos.push_back(uint16_t(r.u16()));
    return r.ok;
}

net::Bytes encode(const Announce& m) {
    W w;
    w.u8('L'), w.u8('Q'), w.u8('B'), w.u8(kProtocol);
    w.u16(m.tcpPort);
    w.u16(m.duration);
    w.u8(m.modifier);
    w.u8(m.players);
    w.str(m.hostName);
    return w.b;
}
bool decode(const net::Bytes& b, Announce& m) {
    R r(b);
    if (r.u8() != 'L' || r.u8() != 'Q' || r.u8() != 'B' || r.u8() != kProtocol) return false;
    m.tcpPort = uint16_t(r.u16());
    m.duration = uint16_t(r.u16());
    m.modifier = uint8_t(r.u8());
    m.players = uint8_t(r.u8());
    m.hostName = r.str();
    return r.ok && m.tcpPort != 0;
}

}  // namespace liq::battle
