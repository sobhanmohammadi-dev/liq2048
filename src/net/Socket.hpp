#pragma once
// Tiny cross-platform socket layer (BSD sockets / Winsock), non-blocking, no threads.
// Used only for the optional local-network Battle mode: a TCP connection between two players and UDP broadcast
// so that a hosted game shows up in the other device's "Browse Games" list. The game never talks to the internet.
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace liq::net {

bool init();  // Winsock start-up (no-op elsewhere); safe to call many times

struct Addr {
    uint32_t ip = 0;  // host byte order, 0x7F000001 = 127.0.0.1
    uint16_t port = 0;
    std::string ipString() const;
};
bool parseIp(const std::string& s, uint32_t& ip);
std::string localIp();  // best guess of this device's LAN address ("127.0.0.1" when unknown)

using Bytes = std::vector<uint8_t>;

// One TCP connection with length-prefixed messages (4-byte little-endian length, then the payload).
class Conn {
public:
    Conn() = default;
    ~Conn() { close(); }
    Conn(const Conn&) = delete;
    Conn& operator=(const Conn&) = delete;
    Conn(Conn&& o) noexcept { *this = std::move(o); }
    Conn& operator=(Conn&& o) noexcept;

    bool connectTo(const Addr& a);  // starts a non-blocking connect
    void adopt(int fd, const Addr& peer);
    void send(const Bytes& payload);  // queued and flushed by pump()
    void pump();                       // flush output, read input, detect closing
    bool pop(Bytes& out);              // next complete message
    void close();
    bool open() const { return fd_ >= 0 && !failed_; }
    bool connecting() const { return connecting_; }
    bool failed() const { return failed_; }
    const Addr& peer() const { return peer_; }
    size_t pendingOut() const { return out_.size() - outPos_; }

private:
    int fd_ = -1;
    bool connecting_ = false, failed_ = false;
    Addr peer_;
    Bytes in_, out_;
    size_t outPos_ = 0;
    std::deque<Bytes> msgs_;
};

class Listener {
public:
    ~Listener() { close(); }
    bool listen(uint16_t port);  // 0 = any free port
    uint16_t port() const { return port_; }
    bool accept(Conn& out);      // non-blocking
    void close();
    bool open() const { return fd_ >= 0; }

private:
    int fd_ = -1;
    uint16_t port_ = 0;
};

class Udp {
public:
    ~Udp() { close(); }
    bool open(uint16_t port, bool reuse);  // port 0 = any
    bool sendTo(const Addr& a, const Bytes& data);
    bool broadcast(uint16_t port, const Bytes& data);  // 255.255.255.255 and, for same-machine tests, 127.0.0.1
    bool recv(Bytes& out, Addr& from);                 // non-blocking
    void close();
    bool isOpen() const { return fd_ >= 0; }

private:
    int fd_ = -1;
};

}  // namespace liq::net
