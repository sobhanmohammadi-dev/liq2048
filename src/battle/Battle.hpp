#pragma once
// Battle mode core: match rules, the computer opponent, and the LAN lobby / link. The game scene only talks to the
// Opponent interface, so a bot match and a LAN match play the same way.
#include <memory>
#include <string>
#include <vector>

#include "battle/Protocol.hpp"
#include "game/World.hpp"
#include "net/Socket.hpp"

namespace liq::battle {

enum class Difficulty { Easy = 0, Medium, Hard, Adaptive };
constexpr int kDifficultyCount = 4;

// Same seed, same index => same drop level, for both players. Bigger drops later in a match, like the solo modes.
int battleLevel(uint32_t seed, int index, int modifier);

struct MatchConfig {
    uint32_t seed = 1;
    int modifier = 0;      // 0 none, 1 low g, 2 heavy, 3 thick, 4 bonus drops
    int duration = 120;    // seconds
    bool lan = false;
    Difficulty difficulty = Difficulty::Medium;
    std::string myName = "PLAYER";
    std::string oppName = "BOT";
};

// What the opponent needs to know about the local player (for Adaptive difficulty and for publishing over LAN).
struct SelfInfo {
    int score = 0, maxLevel = 0, drops = 0;
    bool finished = false, toppedOut = false;
    float elapsed = 0;
    const World* world = nullptr;
    float aimX = 0;
    int heldLevel = 0;
};

class Opponent {
public:
    virtual ~Opponent() = default;
    virtual void update(float dt, const SelfInfo& me) = 0;  // once per frame while the match is on
    virtual int score() const = 0;
    virtual int maxLevel() const = 0;
    virtual bool finished() const = 0;
    virtual bool toppedOut() const { return false; }
    virtual bool disconnected() const { return false; }
    virtual const World& world() const = 0;
    virtual float aimX() const = 0;
    virtual int heldLevel() const = 0;
    virtual float viewAlpha() const { return 1.f; }  // interpolation between the last two pictures of the pot
    virtual const std::string& name() const = 0;
    virtual void fastForward(bool) {}                // the player is out: hurry a simulated opponent along
    virtual void sendFinal(const SelfInfo&) {}       // the player has finished
    virtual void leave() {}                          // the player forfeits / quits
    virtual bool isLan() const { return false; }
};

std::unique_ptr<Opponent> makeBot(const MatchConfig& cfg);

// ---- LAN ----
struct Peer {  // one entry of the "Browse Games" list
    Announce info;
    uint32_t ip = 0;
    double lastSeen = 0;
};

// An established connection between two players, handed from the lobby to the game scene.
struct Link {
    net::Conn conn;
    MatchConfig cfg;
    bool host = false;
};

class HostLobby {
public:
    bool start(const MatchConfig& cfg);  // listen + start announcing
    void update(float dt);
    bool hasGuest() const { return guestJoined_; }
    const std::string& guestName() const { return guestName_; }
    bool failed() const { return failed_ || (guestJoined_ && !link_->conn.open()); }
    uint16_t port() const { return listener_.port(); }
    // Sends Start and gives away the connection.
    std::shared_ptr<Link> begin();
    void cancel();
    const MatchConfig& config() const { return cfg_; }

private:
    MatchConfig cfg_;
    net::Listener listener_;
    net::Udp udp_;
    std::shared_ptr<Link> link_;
    bool guestJoined_ = false, failed_ = false;
    std::string guestName_;
    float announceT_ = 0, helloWait_ = 0;
};

class Browser {
public:
    bool start();
    void update(float dt);
    const std::vector<Peer>& games() const { return games_; }
    void stop() { udp_.close(); }

private:
    net::Udp udp_;
    std::vector<Peer> games_;
    double now_ = 0;
};

class JoinLobby {
public:
    void start(const Peer& p, const std::string& myName);
    void update(float dt);
    enum class State { Connecting, Waiting, Started, Failed };
    State state() const { return state_; }
    const MatchConfig& config() const { return link_->cfg; }
    std::shared_ptr<Link> take() { return link_; }
    void cancel();

private:
    std::shared_ptr<Link> link_;
    State state_ = State::Failed;
    std::string myName_;
    bool sentHello_ = false;
    float timeout_ = 0;
};

std::unique_ptr<Opponent> makeNetOpponent(std::shared_ptr<Link> link);

}  // namespace liq::battle
