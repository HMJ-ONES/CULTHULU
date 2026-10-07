#include "net/Socket.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <errno.h>

namespace cultulhu {
namespace net {
namespace {

bool setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool waitFd(int fd, bool forWrite, int timeoutMs) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    int r = select(fd + 1, forWrite ? nullptr : &set, forWrite ? &set : nullptr,
                   nullptr, timeoutMs < 0 ? nullptr : &tv);
    return r > 0;
}

sockaddr_in makeAddr(const std::string& ip, uint16_t port) {
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &a.sin_addr);
    return a;
}

} // namespace

// ---------------- UdpSocket ----------------

bool UdpSocket::open() {
    close();
    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) return false;
    int one = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    return setNonBlocking(fd_);
}

bool UdpSocket::bind(uint16_t port) {
    if (!valid() && !open()) return false;
    sockaddr_in a = makeAddr("0.0.0.0", port);
    return ::bind(fd_, reinterpret_cast<sockaddr*>(&a), sizeof(a)) == 0;
}

bool UdpSocket::setBroadcast(bool on) {
    int v = on ? 1 : 0;
    return setsockopt(fd_, SOL_SOCKET, SO_BROADCAST, &v, sizeof(v)) == 0;
}

bool UdpSocket::sendTo(const std::string& ip, uint16_t port, const void* data,
                       size_t len) {
    if (!valid()) return false;
    sockaddr_in a = makeAddr(ip, port);
    ssize_t n = ::sendto(fd_, data, len, 0, reinterpret_cast<sockaddr*>(&a),
                         sizeof(a));
    return n == static_cast<ssize_t>(len);
}

long UdpSocket::recvFrom(void* buf, size_t cap, std::string& fromIp,
                         uint16_t& fromPort, int timeoutMs) {
    if (!valid()) return -1;
    if (!waitFd(fd_, false, timeoutMs)) return 0;  // timeout
    sockaddr_in a{};
    socklen_t alen = sizeof(a);
    ssize_t n = ::recvfrom(fd_, buf, cap, 0, reinterpret_cast<sockaddr*>(&a),
                           &alen);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return -1;
    }
    char ipbuf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &a.sin_addr, ipbuf, sizeof(ipbuf));
    fromIp = ipbuf;
    fromPort = ntohs(a.sin_port);
    return static_cast<long>(n);
}

void UdpSocket::close() {
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

// ---------------- TcpSocket ----------------

TcpSocket::TcpSocket(TcpSocket&& o) noexcept : fd_(o.fd_) { o.fd_ = -1; }

TcpSocket& TcpSocket::operator=(TcpSocket&& o) noexcept {
    if (this != &o) { close(); fd_ = o.fd_; o.fd_ = -1; }
    return *this;
}

TcpSocket TcpSocket::adopt(int fd) {
    TcpSocket s;
    s.fd_ = fd;
    setNonBlocking(fd);
    return s;
}

bool TcpSocket::connect(const std::string& ip, uint16_t port, int timeoutMs) {
    close();
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0 || !setNonBlocking(fd_)) { close(); return false; }
    sockaddr_in a = makeAddr(ip, port);
    int r = ::connect(fd_, reinterpret_cast<sockaddr*>(&a), sizeof(a));
    if (r == 0) return true;
    if (errno != EINPROGRESS) { close(); return false; }
    if (!waitFd(fd_, true, timeoutMs)) { close(); return false; }
    int err = 0;
    socklen_t elen = sizeof(err);
    getsockopt(fd_, SOL_SOCKET, SO_ERROR, &err, &elen);
    if (err != 0) { close(); return false; }
    int one = 1;
    setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    return true;
}

bool TcpSocket::sendAll(const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    size_t sent = 0;
    while (sent < len) {
        if (!waitFd(fd_, true, 2000)) return false;
        ssize_t n = ::send(fd_, p + sent, len - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return false;
        }
        if (n == 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

long TcpSocket::recvSome(void* buf, size_t cap, int timeoutMs) {
    if (!waitFd(fd_, false, timeoutMs)) return kRecvTimeout;
    ssize_t n = ::recv(fd_, buf, cap, 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return kRecvTimeout;
        return -1;
    }
    return static_cast<long>(n);  // 0 = orderly shutdown
}

void TcpSocket::close() {
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

// ---------------- TcpListener ----------------

bool TcpListener::listen(uint16_t port, int backlog) {
    close();
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0 || !setNonBlocking(fd_)) { close(); return false; }
    int one = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in a = makeAddr("0.0.0.0", port);
    if (::bind(fd_, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0) {
        close(); return false;
    }
    if (::listen(fd_, backlog) != 0) { close(); return false; }
    return true;
}

uint16_t TcpListener::boundPort() const {
    sockaddr_in a{};
    socklen_t len = sizeof(a);
    if (getsockname(fd_, reinterpret_cast<sockaddr*>(&a), &len) != 0)
        return 0;
    return ntohs(a.sin_port);
}

std::optional<TcpSocket> TcpListener::accept(int timeoutMs) {
    if (!waitFd(fd_, false, timeoutMs)) return std::nullopt;
    int c = ::accept(fd_, nullptr, nullptr);
    if (c < 0) return std::nullopt;
    int one = 1;
    setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    return TcpSocket::adopt(c);
}

void TcpListener::close() {
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

} // namespace net
} // namespace cultulhu
