#include "battle/Battle.hpp"
#include "game/Liquid.hpp"
#include "game/Modifier.hpp"

#include <algorithm>
#include <cmath>

#include "core/Config.hpp"
#include "core/Util.hpp"

namespace liq::battle {

namespace {

uint32_t mix32(uint32_t x) {
    x += 0x9E3779B9u;
    x = (x ^ (x >> 16)) * 0x85EBCA6Bu;
    x = (x ^ (x >> 13)) * 0xC2B2AE35u;
    return x ^ (x >> 16);
}

float gravityFor(int modifier) { return cfg::kGravity * modifierGravity(modifier); }

// ------------------------------------------------------------------------------------------------
// Computer opponent: plays its own hidden pot with the real physics and the same drop queue as the player.
// ------------------------------------------------------------------------------------------------
class Bot final : public Opponent {
public:
    explicit Bot(const MatchConfig& cfg) : cfg_(cfg) {
        w_.substeps = 1;  // cheaper than the player's pot; fairness is about decisions, not liquid detail
        applyLiquid(w_, cfg.liquid, cfg.modifier == kModThick);
        rng_.seed(mix32(cfg.seed ^ 0xB07B07u));
        cur_ = battleLevel(cfg.seed, 0, cfg.modifier);
        next_ = battleLevel(cfg.seed, 1, cfg.modifier);
        idx_ = 2;
        aimX_ = (cfg::kFieldL + cfg::kFieldR) * 0.5f;
        cooldown_ = 1.2f;  // like a human: a moment before the first drop
        pick();
    }

    void update(float dt, const SelfInfo& me) override {
        if (done_) return;
        acc_ += dt * (fast_ ? 8.f : 1.f);
        if (cfg_.difficulty == Difficulty::Adaptive) adapt(dt, me);
        int guard = 0;
        while (acc_ >= cfg::kStep && !done_ && guard++ < 40) {
            tick();
            acc_ -= cfg::kStep;
        }
    }
    int score() const override { return score_; }
    int maxLevel() const override { return maxLevel_; }
    bool finished() const override { return done_; }
    bool toppedOut() const override { return topped_; }
    const World& world() const override { return w_; }
    float aimX() const override { return aimX_; }
    int heldLevel() const override { return cur_; }
    float viewAlpha() const override { return clampv(acc_ / cfg::kStep, 0.f, 1.f); }
    const std::string& name() const override { return cfg_.oppName; }
    void fastForward(bool on) override { fast_ = on; }

private:
    struct Params {
        float tMin, tMax, noise, random, speed, smart;
    };
    Params params() const {
        static const Params easy{1.50f, 2.40f, 42.f, 0.30f, 70.f, 0.35f};
        static const Params medium{0.95f, 1.40f, 20.f, 0.12f, 120.f, 0.70f};
        static const Params hard{0.62f, 0.85f, 6.f, 0.02f, 190.f, 0.98f};
        auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
        switch (cfg_.difficulty) {
            case Difficulty::Easy: return easy;
            case Difficulty::Medium: return medium;
            case Difficulty::Hard: return hard;
            default: {
                const float s = skill_;
                return Params{lerp(easy.tMin, hard.tMin, s), lerp(easy.tMax, hard.tMax, s), lerp(easy.noise, hard.noise, s),
                              lerp(easy.random, hard.random, s), lerp(easy.speed, hard.speed, s), lerp(easy.smart, hard.smart, s)};
            }
        }
    }

    // Adaptive: nudge the skill so the match stays close, whoever is ahead.
    void adapt(float dt, const SelfInfo& me) {
        adaptT_ += dt;
        if (adaptT_ < 4.f) return;
        adaptT_ = 0;
        const float diff = float(me.score - score_);
        const float scale = std::max(300.f, float(std::max(me.score, score_)));
        if (diff > 0.12f * scale) skill_ += 0.08f;
        else if (diff < -0.12f * scale) skill_ -= 0.08f;
        skill_ = clampv(skill_, 0.15f, 0.95f);
    }

    void pick() {
        const Params p = params();
        const float r = World::radiusFor(cur_);
        const float lo = cfg::kFieldL + r, hi = cfg::kFieldR - r;
        float target = rng_.uni(lo, hi);
        if (rng_.uni(0.f, 1.f) >= p.random && rng_.uni(0.f, 1.f) < p.smart) {
            // 1. land on a settled drop of the same level (merge). 2. otherwise fill the lowest part of the pot.
            float bestD = 1e9f;
            bool found = false;
            for (const Drop& d : w_.drops) {
                if (!d.n || d.level != cur_ || d.age < 0.8f || d.level >= 11) continue;
                const float dist = std::fabs(d.x - aimX_) + (cfg::kFloor - d.y) * 0.15f;  // prefer near, deep ones
                if (dist < bestD) {
                    bestD = dist;
                    target = d.x;
                    found = true;
                }
            }
            if (!found) {
                float lowest = -1e9f;
                for (float x = lo; x <= hi; x += 14.f) {
                    const float y = w_.surfaceY(x, r * 0.8f);  // bigger y = lower surface
                    if (y > lowest + 0.5f) {
                        lowest = y;
                        target = x;
                    }
                }
            }
            target += rng_.uni(-p.noise, p.noise);
        } else {
            target += rng_.uni(-p.noise, p.noise) * 0.2f;
        }
        target_ = clampv(target, lo, hi);
    }

    void tick() {
        const Params p = params();
        elapsed_ += cfg::kStep;
        cooldown_ -= cfg::kStep;
        // Steer toward the target, then drop.
        const float d = target_ - aimX_;
        const float stepMax = p.speed * cfg::kStep;
        aimX_ += clampv(d, -stepMax, stepMax);
        if (cooldown_ <= 0.f && std::fabs(target_ - aimX_) < 1.5f && elapsed_ < float(cfg_.duration)) {
            w_.spawn(aimX_, cfg::kSpawnY, cur_);
            cur_ = next_;
            next_ = battleLevel(cfg_.seed, idx_++, cfg_.modifier);
            cooldown_ = rng_.uni(p.tMin, p.tMax);
            pick();
        }
        w_.step(cfg::kStep, gravityFor(cfg_.modifier), modifierSideways(cfg_.modifier, simStep_++));
        for (const auto& e : w_.events) {
            score_ += (1 << e.level) * e.chain;
            maxLevel_ = std::max(maxLevel_, e.level);
        }
        w_.events.clear();
        w_.impacts.clear();
        w_.updateOverflow(cfg::kDeadY + 22.f, cfg::kStep);
        if (w_.overflow() > cfg::kOverflowSeconds) {
            topped_ = done_ = true;
        } else if (elapsed_ >= float(cfg_.duration)) {
            done_ = true;
        }
    }

    MatchConfig cfg_;
    World w_;
    Rng rng_;
    float acc_ = 0, elapsed_ = 0, cooldown_ = 0, aimX_ = 0, target_ = 0, adaptT_ = 0, skill_ = 0.45f;
    uint32_t simStep_ = 0;
    int idx_ = 0, cur_ = 0, next_ = 0, score_ = 0, maxLevel_ = 0;
    bool done_ = false, topped_ = false, fast_ = false;
};

// ------------------------------------------------------------------------------------------------
// The other human, over the LAN connection.
// ------------------------------------------------------------------------------------------------
class NetOpponent final : public Opponent {
public:
    explicit NetOpponent(std::shared_ptr<Link> link) : link_(std::move(link)) {}

    void update(float dt, const SelfInfo& me) override {
        net::Conn& c = link_->conn;
        c.pump();
        net::Bytes m;
        while (c.pop(m)) {
            silent_ = 0;
            switch (typeOf(m)) {
                case Msg::Frame: {
                    Frame f;
                    if (!decode(m, f)) break;
                    score_ = f.score;
                    maxLevel_ = f.maxLevel;
                    if (f.flags & 1) finished_ = true;
                    if (f.flags & 2) topped_ = true;
                    aim_ = float(f.snap.aimX) / 128.f;
                    held_ = f.snap.heldLevel;
                    std::vector<int> lv, cn;
                    for (const auto& d : f.snap.drops) {
                        lv.push_back(d.level);
                        cn.push_back(d.n);
                    }
                    view_.setPicture(lv, cn, f.snap.pos);
                    sinceFrame_ = 0.f;
                    break;
                }
                case Msg::Final: {
                    Final f;
                    if (!decode(m, f)) break;
                    score_ = f.score;
                    maxLevel_ = f.maxLevel;
                    topped_ = f.toppedOut != 0;
                    finished_ = true;
                    break;
                }
                case Msg::Leave: left_ = true; break;
                default: break;
            }
        }
        silent_ += dt;
        sinceFrame_ += dt;
        if (!finished_ && (c.failed() || silent_ > 6.f)) left_ = true;
        // Publish a picture of the local pot ten times a second.
        pubT_ -= dt;
        if (pubT_ <= 0.f && c.open() && !c.connecting()) {
            pubT_ = 0.1f;
            Frame f;
            f.score = me.score;
            f.maxLevel = uint8_t(std::min(me.maxLevel, 11));
            f.drops = uint16_t(std::min(me.drops, 65535));
            f.flags = uint8_t((me.finished ? 1 : 0) | (me.toppedOut ? 2 : 0));
            f.snap.aimX = uint16_t(clampv(me.aimX, 0.f, 500.f) * 128.f);
            f.snap.heldLevel = uint8_t(clampv(me.heldLevel, 0, 11));
            if (me.world) {
                std::vector<int> lv, cn;
                me.world->picture(lv, cn, f.snap.pos);
                for (size_t i = 0; i < lv.size(); ++i) f.snap.drops.push_back({uint8_t(lv[i]), uint16_t(cn[i])});
            }
            c.send(encode(f));
            c.pump();
        }
    }
    int score() const override { return score_; }
    int maxLevel() const override { return maxLevel_; }
    bool finished() const override { return finished_ || left_; }
    bool toppedOut() const override { return topped_; }
    bool disconnected() const override { return left_ && !finished_; }
    const World& world() const override { return view_; }
    float aimX() const override { return aim_; }
    int heldLevel() const override { return held_; }
    float viewAlpha() const override { return clampv(sinceFrame_ / 0.1f, 0.f, 1.f); }
    const std::string& name() const override { return link_->cfg.oppName; }
    void sendFinal(const SelfInfo& me) override {
        Final f;
        f.score = me.score;
        f.maxLevel = uint8_t(std::min(me.maxLevel, 11));
        f.toppedOut = me.toppedOut ? 1 : 0;
        link_->conn.send(encode(f));
        link_->conn.pump();
    }
    void leave() override {
        link_->conn.send(encodeLeave());
        link_->conn.pump();
        link_->conn.close();
    }
    bool isLan() const override { return true; }

private:
    std::shared_ptr<Link> link_;
    World view_;
    int score_ = 0, maxLevel_ = 0, held_ = 0;
    float aim_ = 135.f, silent_ = 0, pubT_ = 0, sinceFrame_ = 0;
    bool finished_ = false, topped_ = false, left_ = false;
};

}  // namespace

int battleLevel(uint32_t seed, int index, int modifier) {
    const int r = int(mix32(seed * 2654435761u + uint32_t(index) * 40503u + 17u) % 100u);
    if (modifier == 4) return r < 15 ? 1 : r < 45 ? 2 : r < 80 ? 3 : 4;
    static const int w[5][6] = {{38, 70, 90, 100, 100, 100}, {28, 56, 78, 93, 100, 100}, {20, 42, 66, 86, 100, 100},
                                {14, 32, 54, 78, 100, 100}, {10, 24, 44, 68, 88, 100}};
    const int tier = std::min(4, std::max(0, index) / 12);
    int lvl = 0;
    while (lvl < 5 && r >= w[tier][lvl]) ++lvl;
    return lvl;
}

std::unique_ptr<Opponent> makeBot(const MatchConfig& cfg) { return std::make_unique<Bot>(cfg); }
std::unique_ptr<Opponent> makeNetOpponent(std::shared_ptr<Link> link) { return std::make_unique<NetOpponent>(std::move(link)); }

// ---- LAN lobby ----
bool HostLobby::start(const MatchConfig& cfg) {
    cfg_ = cfg;
    cfg_.lan = true;
    link_ = std::make_shared<Link>();
    link_->host = true;
    guestJoined_ = failed_ = false;
    if (!listener_.listen(0) || !udp_.open(0, false)) {
        failed_ = true;
        return false;
    }
    announceT_ = 0;
    return true;
}

void HostLobby::update(float dt) {
    if (failed_) return;
    announceT_ -= dt;
    if (announceT_ <= 0.f) {
        announceT_ = 1.f;
        Announce a;
        a.hostName = cfg_.myName;
        a.tcpPort = listener_.port();
        a.duration = uint16_t(cfg_.duration);
        a.modifier = uint8_t(cfg_.modifier);
        a.liquid = uint8_t(cfg_.liquid);
        a.players = guestJoined_ ? 2 : 1;
        udp_.broadcast(kDiscoveryPort, encode(a));
    }
    net::Conn incoming;
    while (listener_.accept(incoming)) {
        if (guestJoined_ || link_->conn.open()) {  // already has a guest (or one is still introducing itself)
            incoming.close();
            continue;
        }
        link_->conn = std::move(incoming);
        helloWait_ = 0;
    }
    if (link_->conn.open() || link_->conn.failed()) {
        link_->conn.pump();
        net::Bytes m;
        while (!guestJoined_ && link_->conn.pop(m)) {
            Hello h;
            if (typeOf(m) == Msg::Hello && decode(m, h)) {
                guestName_ = h.name.empty() ? "PLAYER" : h.name;
                Welcome w;
                w.seed = cfg_.seed;
                w.modifier = uint8_t(cfg_.modifier);
                w.liquid = uint8_t(cfg_.liquid);
                w.duration = uint16_t(cfg_.duration);
                w.hostName = cfg_.myName;
                link_->conn.send(encode(w));
                link_->conn.pump();
                guestJoined_ = true;
            } else {
                link_->conn.close();
            }
        }
        helloWait_ += dt;
        if (!guestJoined_ && helloWait_ > 5.f) link_->conn.close();
        if (link_->conn.failed()) {  // the guest left again: be open for the next one
            link_->conn.close();
            guestJoined_ = false;
        }
    }
}

std::shared_ptr<Link> HostLobby::begin() {
    if (!guestJoined_) return nullptr;
    link_->conn.send(encodeStart());
    link_->conn.pump();
    link_->cfg = cfg_;
    link_->cfg.oppName = guestName_;
    udp_.close();
    listener_.close();
    return link_;
}

void HostLobby::cancel() {
    udp_.close();
    listener_.close();
    if (link_) link_->conn.close();
}

bool Browser::start() {
    games_.clear();
    return udp_.open(kDiscoveryPort, true);
}

void Browser::update(float dt) {
    now_ += dt;
    net::Bytes b;
    net::Addr from;
    while (udp_.recv(b, from)) {
        Announce a;
        if (!decode(b, a)) continue;
        bool known = false;
        for (Peer& p : games_)
            if (p.ip == from.ip && p.info.tcpPort == a.tcpPort) {
                p.info = a;
                p.lastSeen = now_;
                known = true;
            }
        if (!known && games_.size() < 12) games_.push_back({a, from.ip, now_});
    }
    games_.erase(std::remove_if(games_.begin(), games_.end(), [&](const Peer& p) { return now_ - p.lastSeen > 3.5; }), games_.end());
}

void JoinLobby::start(const Peer& p, const std::string& myName) {
    link_ = std::make_shared<Link>();
    myName_ = myName;
    sentHello_ = false;
    timeout_ = 0;
    state_ = State::Connecting;
    if (!link_->conn.connectTo(net::Addr{p.ip, p.info.tcpPort})) state_ = State::Failed;
}

void JoinLobby::update(float dt) {
    if (state_ == State::Failed || state_ == State::Started || !link_) return;
    net::Conn& c = link_->conn;
    c.pump();
    timeout_ += dt;
    if (c.failed() || (state_ == State::Connecting && timeout_ > 6.f)) {
        state_ = State::Failed;
        return;
    }
    if (!c.connecting() && !sentHello_) {
        Hello h;
        h.name = myName_;
        c.send(encode(h));
        c.pump();
        sentHello_ = true;
    }
    net::Bytes m;
    while (c.pop(m)) {
        Welcome w;
        if (typeOf(m) == Msg::Welcome && decode(m, w)) {
            link_->cfg.seed = w.seed;
            link_->cfg.modifier = w.modifier;
            link_->cfg.liquid = w.liquid;  // the host's liquid replaces mine for this match
            link_->cfg.duration = w.duration;
            link_->cfg.lan = true;
            link_->cfg.myName = myName_;
            link_->cfg.oppName = w.hostName;
            state_ = State::Waiting;
        } else if (typeOf(m) == Msg::Start && state_ == State::Waiting) {
            state_ = State::Started;
        } else if (typeOf(m) == Msg::Leave) {
            state_ = State::Failed;
        }
    }
}

void JoinLobby::cancel() {
    if (link_) {
        link_->conn.send(encodeLeave());
        link_->conn.pump();
        link_->conn.close();
    }
    state_ = State::Failed;
}

}  // namespace liq::battle
