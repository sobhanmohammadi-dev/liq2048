
#define _CRT_SECURE_NO_WARNINGS

#include "net/Socket.hpp"

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using sock_t = SOCKET;
static void closeSock(int fd) { closesocket(SOCKET(fd)); }
static int lastErr() { return WSAGetLastError(); }
static bool inProgress(int e) { return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS; }
static bool wouldBlock(int e) { return e == WSAEWOULDBLOCK; }
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using sock_t = int;
static void closeSock(int fd) { ::close(fd); }
static int lastErr() { return errno; }
static bool inProgress(int e) { return e == EINPROGRESS || e == EWOULDBLOCK || e == EAGAIN; }
static bool wouldBlock(int e) { return e == EWOULDBLOCK || e == EAGAIN; }
#endif

namespace liq::net {

namespace {

void setNonBlocking(int fd) {
#ifdef _WIN32
    u_long m = 1;
    ioctlsocket(SOCKET(fd), FIONBIO, &m);
#else
    const int fl = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
#endif
}

sockaddr_in makeAddr(uint32_t ip, uint16_t port) {
    sockaddr_in sa;
    std::memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(ip);
    sa.sin_port = htons(port);
    return sa;
}

void noSigPipe(int fd) {
#ifdef SO_NOSIGPIPE
    int one = 1;
    setsockopt(sock_t(fd), SOL_SOCKET, SO_NOSIGPIPE, reinterpret_cast<const char*>(&one), sizeof one);
#else
    (void)fd;
#endif
}

#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
constexpr int kSendFlags = 0;
#endif

}  // namespace

bool init() {
#ifdef _WIN32
    static bool ok = [] {
        WSADATA d;
        return WSAStartup(MAKEWORD(2, 2), &d) == 0;
    }();
    return ok;
#else
    return true;
#endif
}

std::string Addr::ipString() const {
    char b[32];
    std::snprintf(b, sizeof b, "%u.%u.%u.%u", (ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
    return b;
}

bool parseIp(const std::string& s, uint32_t& ip) {
    unsigned a, b, c, d;
    char tail;
    if (std::sscanf(s.c_str(), "%u.%u.%u.%u%c",&a, &b, &c, &d, &tail) != 4 ||a > 255 || b > 255 || c > 255 || d > 255) {return false;}
    ip = (a << 24) | (b << 16) | (c << 8) | d;
    return true;
}

std::string localIp() {
    init();
    std::string result = "127.0.0.1";
    sock_t s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s == sock_t(-1)) return result;
    sockaddr_in to = makeAddr(0x0AFFFFFF, 9);  // a private address: connect() on UDP sends nothing
    if (::connect(s, reinterpret_cast<sockaddr*>(&to), sizeof to) == 0) {
        sockaddr_in me;
        socklen_t len = sizeof me;
        if (::getsockname(s, reinterpret_cast<sockaddr*>(&me), &len) == 0) {
            Addr a;
            a.ip = ntohl(me.sin_addr.s_addr);
            if (a.ip != 0) result = a.ipString();
        }
    }
    closeSock(int(s));
    return result;
}

// ---- Conn ----
Conn& Conn::operator=(Conn&& o) noexcept {
    if (this != &o) {
        close();
        fd_ = o.fd_;
        connecting_ = o.connecting_;
        failed_ = o.failed_;
        peer_ = o.peer_;
        in_ = std::move(o.in_);
        out_ = std::move(o.out_);
        outPos_ = o.outPos_;
        msgs_ = std::move(o.msgs_);
        o.fd_ = -1;
    }
    return *this;
}

bool Conn::connectTo(const Addr& a) {
    close();
    init();
    sock_t s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == sock_t(-1)) {
        failed_ = true;
        return false;
    }
    fd_ = int(s);
    setNonBlocking(fd_);
    noSigPipe(fd_);
    int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one), sizeof one);
    peer_ = a;
    failed_ = false;
    sockaddr_in sa = makeAddr(a.ip, a.port);
    const int r = ::connect(s, reinterpret_cast<sockaddr*>(&sa), sizeof sa);
    if (r == 0) {
        connecting_ = false;
    } else if (inProgress(lastErr())) {
        connecting_ = true;
    } else {
        failed_ = true;
        closeSock(fd_);
        fd_ = -1;
        return false;
    }
    return true;
}

void Conn::adopt(int fd, const Addr& peer) {
    close();
    fd_ = fd;
    peer_ = peer;
    failed_ = connecting_ = false;
    setNonBlocking(fd_);
    noSigPipe(fd_);
    int one = 1;
    setsockopt(sock_t(fd_), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one), sizeof one);
}

void Conn::send(const Bytes& payload) {
    if (fd_ < 0 || failed_ || payload.size() > (1u << 20)) return;
    const uint32_t n = uint32_t(payload.size());
    for (int i = 0; i < 4; ++i) out_.push_back(uint8_t(n >> (8 * i)));
    out_.insert(out_.end(), payload.begin(), payload.end());
}

void Conn::pump() {
    if (fd_ < 0 || failed_) return;
    if (connecting_) {  // finished when the socket becomes writable
        fd_set w, e;
        FD_ZERO(&w);
        FD_ZERO(&e);
        FD_SET(sock_t(fd_), &w);
        FD_SET(sock_t(fd_), &e);
        timeval tv{0, 0};
        const int r = ::select(fd_ + 1, nullptr, &w, &e, &tv);
        if (r > 0) {
            int err = 0;
            socklen_t len = sizeof err;
            getsockopt(sock_t(fd_), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
            if (err != 0 || FD_ISSET(sock_t(fd_), &e)) {
                failed_ = true;
                connecting_ = false;
                return;
            }
            connecting_ = false;
        } else {
            return;
        }
    }
    while (outPos_ < out_.size()) {  // flush
        const int n = int(::send(sock_t(fd_), reinterpret_cast<const char*>(out_.data() + outPos_), int(out_.size() - outPos_), kSendFlags));
        if (n > 0) {
            outPos_ += size_t(n);
        } else {
            if (n < 0 && wouldBlock(lastErr())) break;
            failed_ = true;
            return;
        }
    }
    if (outPos_ == out_.size()) {
        out_.clear();
        outPos_ = 0;
    }
    uint8_t buf[8192];
    for (;;) {  // read
        const int n = int(::recv(sock_t(fd_), reinterpret_cast<char*>(buf), sizeof buf, 0));
        if (n > 0) {
            in_.insert(in_.end(), buf, buf + n);
        } else if (n == 0) {
            failed_ = true;  // closed by the peer
            break;
        } else {
            if (!wouldBlock(lastErr())) failed_ = true;
            break;
        }
    }
    for (;;) {  // split into messages
        if (in_.size() < 4) break;
        const uint32_t len = uint32_t(in_[0]) | uint32_t(in_[1]) << 8 | uint32_t(in_[2]) << 16 | uint32_t(in_[3]) << 24;
        if (len > (1u << 20)) {
            failed_ = true;
            in_.clear();
            break;
        }
        if (in_.size() < 4 + size_t(len)) break;
        msgs_.emplace_back(in_.begin() + 4, in_.begin() + 4 + long(len));
        in_.erase(in_.begin(), in_.begin() + 4 + long(len));
    }
}

bool Conn::pop(Bytes& out) {
    if (msgs_.empty()) return false;
    out = std::move(msgs_.front());
    msgs_.pop_front();
    return true;
}

void Conn::close() {
    if (fd_ >= 0) closeSock(fd_);
    fd_ = -1;
    connecting_ = false;
    in_.clear();
    out_.clear();
    outPos_ = 0;
    // msgs_ stays readable: a final message may arrive together with the close.
}

// ---- Listener ----
bool Listener::listen(uint16_t port) {
    close();
    init();
    sock_t s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == sock_t(-1)) return false;
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&one), sizeof one);
    sockaddr_in sa = makeAddr(0, port);
    if (::bind(s, reinterpret_cast<sockaddr*>(&sa), sizeof sa) != 0 || ::listen(s, 4) != 0) {
        closeSock(int(s));
        return false;
    }
    sockaddr_in me;
    socklen_t len = sizeof me;
    ::getsockname(s, reinterpret_cast<sockaddr*>(&me), &len);
    port_ = ntohs(me.sin_port);
    fd_ = int(s);
    setNonBlocking(fd_);
    return true;
}

bool Listener::accept(Conn& out) {
    if (fd_ < 0) return false;
    sockaddr_in from;
    socklen_t len = sizeof from;
    sock_t c = ::accept(sock_t(fd_), reinterpret_cast<sockaddr*>(&from), &len);
    if (c == sock_t(-1)) return false;
    Addr a;
    a.ip = ntohl(from.sin_addr.s_addr);
    a.port = ntohs(from.sin_port);
    out.adopt(int(c), a);
    return true;
}

void Listener::close() {
    if (fd_ >= 0) closeSock(fd_);
    fd_ = -1;
}

// ---- Udp ----
bool Udp::open(uint16_t port, bool reuse) {
    close();
    init();
    sock_t s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s == sock_t(-1)) return false;
    int one = 1;
    if (reuse) {
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&one), sizeof one);
#ifdef SO_REUSEPORT
        setsockopt(s, SOL_SOCKET, SO_REUSEPORT, reinterpret_cast<const char*>(&one), sizeof one);
#endif
    }
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&one), sizeof one);
    sockaddr_in sa = makeAddr(0, port);
    if (::bind(s, reinterpret_cast<sockaddr*>(&sa), sizeof sa) != 0) {
        closeSock(int(s));
        return false;
    }
    fd_ = int(s);
    setNonBlocking(fd_);
    return true;
}

bool Udp::sendTo(const Addr& a, const Bytes& data) {
    if (fd_ < 0) return false;
    sockaddr_in sa = makeAddr(a.ip, a.port);
    return ::sendto(sock_t(fd_), reinterpret_cast<const char*>(data.data()), int(data.size()), kSendFlags, reinterpret_cast<sockaddr*>(&sa),
                    sizeof sa) >= 0;
}

bool Udp::broadcast(uint16_t port, const Bytes& data) {
    bool ok = sendTo(Addr{0xFFFFFFFFu, port}, data);
    ok = sendTo(Addr{0x7F000001u, port}, data) || ok;
    return ok;
}

bool Udp::recv(Bytes& out, Addr& from) {
    if (fd_ < 0) return false;
    uint8_t buf[2048];
    sockaddr_in sa;
    socklen_t len = sizeof sa;
    const int n = int(::recvfrom(sock_t(fd_), reinterpret_cast<char*>(buf), sizeof buf, 0, reinterpret_cast<sockaddr*>(&sa), &len));
    if (n < 0) return false;
    out.assign(buf, buf + n);
    from.ip = ntohl(sa.sin_addr.s_addr);
    from.port = ntohs(sa.sin_port);
    return true;
}

void Udp::close() {
    if (fd_ >= 0) closeSock(fd_);
    fd_ = -1;
}

}  // namespace liq::net
