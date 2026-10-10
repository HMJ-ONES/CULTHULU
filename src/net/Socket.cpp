#include "net/Socket.h"

#ifdef _WIN32
// Winsock2: winsock2.h must come before windows.h (it includes it).
// ws2tcpip.h provides inet_pton/inet_ntop/getaddrinfo on Windows.
// The #pragma comment lines are belt-and-suspenders: CMake also links
// ws2_32/iphlpapi (see root CMakeLists.txt), the linker dedupes.
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <cstring>
#ifdef _WIN32
// No errno on the Winsock path; WSAGetLastError() is used instead.
#else
#include <errno.h>
#endif

namespace cultulhu {
namespace net {
namespace {

#ifdef _WIN32
// Winsock2: WSAStartup() must run once per process before any socket call.
// Function-local static: initialized exactly once, thread-safe (C++11),
// WSACleanup() runs at process exit.
struct WsaGuard {
    WsaGuard() {
        WSADATA d{};
        ok_ = (WSAStartup(MAKEWORD(2, 2), &d) == 0);
    }
    ~WsaGuard() {
        if (ok_) WSACleanup();
    }
    bool ok_ = false;
};

bool ensureWsa() {
    static WsaGuard g;
    return g.ok_;
}

// Winsock2 error mapping: WSAGetLastError() instead of errno.
// A non-blocking connect() surfaces as WSAEWOULDBLOCK (there is no
// distinct EINPROGRESS on Winsock), so both map to the same constant.
inline int lastError() { return WSAGetLastError(); }
constexpr int kErrWouldBlock = WSAEWOULDBLOCK;
constexpr int kErrInProgress = WSAEWOULDBLOCK;
// Winsock send()/recv() take int lengths (POSIX: size_t). Our frames are
// small; the cast is made explicit at each call site.
#else
inline int lastError() { return errno; }
constexpr int kErrWouldBlock = EWOULDBLOCK;  // == EAGAIN on Linux
constexpr int kErrInProgress = EINPROGRESS;
#endif

bool setNonBlocking(NativeSocket fd) {
#ifdef _WIN32
    // Winsock2 equivalent of fcntl(fd, F_SETFL, O_NONBLOCK).
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

void closeNative(NativeSocket fd) {
    if (fd == kInvalidNativeSocket) return;
#ifdef _WIN32
    // Winsock2: closesocket(), not ::close().
    closesocket(fd);
#else
    ::close(fd);
#endif
}

bool waitFd(NativeSocket fd, bool forWrite, int timeoutMs) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
#ifdef _WIN32
    // Winsock ignores the first select() arg (nfds): pass 0.
    int r = select(0, forWrite ? nullptr : &set, forWrite ? &set : nullptr,
                   nullptr, timeoutMs < 0 ? nullptr : &tv);
#else
    int r = select(fd + 1, forWrite ? nullptr : &set, forWrite ? &set : nullptr,
                   nullptr, timeoutMs < 0 ? nullptr : &tv);
#endif
    return r > 0;
}

sockaddr_in makeAddr(const std::string& ip, uint16_t port) {
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &a.sin_addr);
    return a;
}

#ifdef _WIN32
// Winsock setsockopt() takes const char* optval (POSIX: const void*).
inline const char* sockOpt(const void* p) {
    return static_cast<const char*>(p);
}
inline char* sockOpt(void* p) { return static_cast<char*>(p); }
// Winsock socklen parameters are int* (POSIX: socklen_t*).
using SockLen = int;
#else
using SockLen = socklen_t;
#endif

} // namespace

// ---------------- UdpSocket ----------------

bool UdpSocket::open() {
#ifdef _WIN32
    if (!ensureWsa()) return false;
#endif
    close();
    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ == kInvalidNativeSocket) return false;
    int one = 1;
#ifdef _WIN32
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, sockOpt(&one), sizeof(one));
#else
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
#endif
    return setNonBlocking(fd_);
}

bool UdpSocket::bind(uint16_t port) {
    if (!valid() && !open()) return false;
    sockaddr_in a = makeAddr("0.0.0.0", port);
    return ::bind(fd_, reinterpret_cast<sockaddr*>(&a), sizeof(a)) == 0;
}

bool UdpSocket::setBroadcast(bool on) {
    int v = on ? 1 : 0;
#ifdef _WIN32
    return setsockopt(fd_, SOL_SOCKET, SO_BROADCAST, sockOpt(&v), sizeof(v)) == 0;
#else
    return setsockopt(fd_, SOL_SOCKET, SO_BROADCAST, &v, sizeof(v)) == 0;
#endif
}

bool UdpSocket::sendTo(const std::string& ip, uint16_t port, const void* data,
                       size_t len) {
    if (!valid()) return false;
    sockaddr_in a = makeAddr(ip, port);
#ifdef _WIN32
    int n = ::sendto(fd_, static_cast<const char*>(data),
                     static_cast<int>(len), 0, reinterpret_cast<sockaddr*>(&a),
                     sizeof(a));
#else
    ssize_t n = ::sendto(fd_, data, len, 0, reinterpret_cast<sockaddr*>(&a),
                         sizeof(a));
#endif
    return n == static_cast<decltype(n)>(len);
}

long UdpSocket::recvFrom(void* buf, size_t cap, std::string& fromIp,
                         uint16_t& fromPort, int timeoutMs) {
    if (!valid()) return -1;
    if (!waitFd(fd_, false, timeoutMs)) return 0;  // timeout
    sockaddr_in a{};
    SockLen alen = sizeof(a);
#ifdef _WIN32
    int n = ::recvfrom(fd_, static_cast<char*>(buf), static_cast<int>(cap), 0,
                       reinterpret_cast<sockaddr*>(&a), &alen);
#else
    ssize_t n = ::recvfrom(fd_, buf, cap, 0, reinterpret_cast<sockaddr*>(&a),
                           &alen);
#endif
    if (n < 0) {
        // Winsock2: WSAEWOULDBLOCK covers both EAGAIN and EWOULDBLOCK.
        if (lastError() == kErrWouldBlock) return 0;
        return -1;
    }
    char ipbuf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &a.sin_addr, ipbuf, sizeof(ipbuf));
    fromIp = ipbuf;
    fromPort = ntohs(a.sin_port);
    return static_cast<long>(n);
}

void UdpSocket::close() {
    closeNative(fd_);
    fd_ = kInvalidNativeSocket;
}

// ---------------- TcpSocket ----------------

TcpSocket::TcpSocket(TcpSocket&& o) noexcept : fd_(o.fd_) {
    o.fd_ = kInvalidNativeSocket;
}

TcpSocket& TcpSocket::operator=(TcpSocket&& o) noexcept {
    if (this != &o) { close(); fd_ = o.fd_; o.fd_ = kInvalidNativeSocket; }
    return *this;
}

TcpSocket TcpSocket::adopt(NativeSocket fd) {
    TcpSocket s;
    s.fd_ = fd;
    setNonBlocking(fd);
    return s;
}

bool TcpSocket::connect(const std::string& ip, uint16_t port, int timeoutMs) {
#ifdef _WIN32
    if (!ensureWsa()) return false;
#endif
    close();
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ == kInvalidNativeSocket || !setNonBlocking(fd_)) {
        close();
        return false;
    }
    sockaddr_in a = makeAddr(ip, port);
    int r = ::connect(fd_, reinterpret_cast<sockaddr*>(&a), sizeof(a));
    if (r == 0) return true;
    // POSIX: EINPROGRESS. Winsock2: WSAEWOULDBLOCK (same constant here).
    if (lastError() != kErrInProgress) { close(); return false; }
    if (!waitFd(fd_, true, timeoutMs)) { close(); return false; }
    int err = 0;
    SockLen elen = sizeof(err);
#ifdef _WIN32
    getsockopt(fd_, SOL_SOCKET, SO_ERROR, sockOpt(&err), &elen);
#else
    getsockopt(fd_, SOL_SOCKET, SO_ERROR, &err, &elen);
#endif
    if (err != 0) { close(); return false; }
    int one = 1;
#ifdef _WIN32
    setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, sockOpt(&one), sizeof(one));
#else
    setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
#endif
    return true;
}

bool TcpSocket::sendAll(const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    size_t sent = 0;
    while (sent < len) {
        if (!waitFd(fd_, true, 2000)) return false;
#ifdef _WIN32
        // Winsock2: no MSG_NOSIGNAL (flags 0); a reset peer surfaces as a
        // normal error return, which the caller already treats as failure.
        int n = ::send(fd_, reinterpret_cast<const char*>(p + sent),
                       static_cast<int>(len - sent), 0);
#else
        ssize_t n = ::send(fd_, p + sent, len - sent, MSG_NOSIGNAL);
#endif
        if (n < 0) {
            if (lastError() == kErrWouldBlock) continue;
            return false;
        }
        if (n == 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

long TcpSocket::recvSome(void* buf, size_t cap, int timeoutMs) {
    if (!waitFd(fd_, false, timeoutMs)) return kRecvTimeout;
#ifdef _WIN32
    int n = ::recv(fd_, static_cast<char*>(buf), static_cast<int>(cap), 0);
#else
    ssize_t n = ::recv(fd_, buf, cap, 0);
#endif
    if (n < 0) {
        if (lastError() == kErrWouldBlock) return kRecvTimeout;
        return -1;
    }
    return static_cast<long>(n);  // 0 = orderly shutdown
}

void TcpSocket::close() {
    closeNative(fd_);
    fd_ = kInvalidNativeSocket;
}

// ---------------- TcpListener ----------------

bool TcpListener::listen(uint16_t port, int backlog) {
#ifdef _WIN32
    if (!ensureWsa()) return false;
#endif
    close();
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ == kInvalidNativeSocket || !setNonBlocking(fd_)) {
        close();
        return false;
    }
    int one = 1;
#ifdef _WIN32
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, sockOpt(&one), sizeof(one));
#else
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
#endif
    sockaddr_in a = makeAddr("0.0.0.0", port);
    if (::bind(fd_, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0) {
        close(); return false;
    }
    if (::listen(fd_, backlog) != 0) { close(); return false; }
    return true;
}

uint16_t TcpListener::boundPort() const {
    sockaddr_in a{};
    SockLen len = sizeof(a);
    if (getsockname(fd_, reinterpret_cast<sockaddr*>(&a), &len) != 0)
        return 0;
    return ntohs(a.sin_port);
}

std::optional<TcpSocket> TcpListener::accept(int timeoutMs) {
    if (!waitFd(fd_, false, timeoutMs)) return std::nullopt;
    NativeSocket c = ::accept(fd_, nullptr, nullptr);
    if (c == kInvalidNativeSocket) return std::nullopt;
    int one = 1;
#ifdef _WIN32
    setsockopt(c, IPPROTO_TCP, TCP_NODELAY, sockOpt(&one), sizeof(one));
#else
    setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
#endif
    return TcpSocket::adopt(c);
}

void TcpListener::close() {
    closeNative(fd_);
    fd_ = kInvalidNativeSocket;
}

} // namespace net
} // namespace cultulhu
