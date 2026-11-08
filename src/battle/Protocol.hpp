#pragma once
// Wire format of a LAN Battle match. Every message is one length-prefixed packet (see net/Socket.hpp) whose first
// byte is the type. Both players run their own pot; what travels is only each pot's score and a picture of it
// (quantised particle positions) ten times a second, so the opponent view is exact on every platform even though
// floating point physics is not bit-identical between devices.
#include <cstdint>
#include <string>
#include <vector>

#include "net/Socket.hpp"

namespace liq::battle {

constexpr uint8_t kProtocol = 2;  // 2: Welcome and Announce carry the host's liquid type (1.8.1)
constexpr uint16_t kDiscoveryPort = 47821;  // UDP: hosts announce, browsers listen

enum class Msg : uint8_t { Hello = 1, Welcome, Start, Frame, Final, Leave };

struct Hello {  // joining player -> host
    std::string name;
};
struct Welcome {  // host -> joining player: the rules of the match
    uint32_t seed = 0;
    uint8_t modifier = 0;
    uint8_t liquid = 0;       // the host's liquid type
    uint16_t duration = 120;  // seconds
    std::string hostName;
};

// A picture of one pot.
struct SnapDrop {
    uint8_t level = 0;
    uint16_t n = 0;  // particles of this drop (they follow in `pos`, drop after drop)
};
struct Snapshot {
    std::vector<SnapDrop> drops;
    std::vector<uint16_t> pos;  // x, y pairs, 1/128 px
    uint16_t aimX = 0;          // 1/128 px
    uint8_t heldLevel = 0;
};
struct Frame {
    int32_t score = 0;
    uint8_t maxLevel = 0;
    uint16_t drops = 0;
    uint8_t flags = 0;  // bit0 finished, bit1 topped out
    Snapshot snap;
};
struct Final {
    int32_t score = 0;
    uint8_t maxLevel = 0;
    uint8_t toppedOut = 0;
};

net::Bytes encode(const Hello& m);
net::Bytes encode(const Welcome& m);
net::Bytes encodeStart();
net::Bytes encode(const Frame& m);
net::Bytes encode(const Final& m);
net::Bytes encodeLeave();

bool decode(const net::Bytes& b, Hello& m);
bool decode(const net::Bytes& b, Welcome& m);
bool decode(const net::Bytes& b, Frame& m);
bool decode(const net::Bytes& b, Final& m);
inline Msg typeOf(const net::Bytes& b) { return b.empty() ? Msg(0) : Msg(b[0]); }

// UDP announcement of a hosted game (broadcast every second while waiting).
struct Announce {
    std::string hostName;
    uint16_t tcpPort = 0;
    uint16_t duration = 120;
    uint8_t modifier = 0;
    uint8_t liquid = 0;
    uint8_t players = 1;  // 1 = open, 2 = full
};
net::Bytes encode(const Announce& m);
bool decode(const net::Bytes& b, Announce& m);

}  // namespace liq::battle
