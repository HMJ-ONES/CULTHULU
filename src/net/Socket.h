#pragma once

// Lightweight socket wrappers for CULT-ULHU multiplayer.
// Engine-agnostic. Dual POSIX / Windows (Winsock2) implementation.
//
// PORTING NOTES (Windows):
//  - Winsock2: SOCKET is an unsigned handle type; INVALID_SOCKET is the
//    "no socket" sentinel (not -1). NativeSocket aliases it.
//  - WSAStartup() is called once per process via a function-local static
//    guard (ensureWsa()) before any socket call; WSACleanup() runs at exit.
//  - closesocket() instead of ::close(); ioctlsocket(FIONBIO) instead of
//    fcntl(O_NONBLOCK); WSAGetLastError() instead of errno.
//  - select()'s first arg (nfds) is ignored by Winsock: pass 0.
//  - Non-blocking connect() on Winsock fails with WSAEWOULDBLOCK (not a
//    distinct EINPROGRESS); SO_ERROR is then polled the same way.
//  - send()/recv()/sendto()/recvfrom() take int lengths on Winsock vs
//    size_t on POSIX: narrow with an explicit cast at the boundary.
//  - MSG_NOSIGNAL does not exist on Winsock: flags are 0 there (a reset
//    peer surfaces as a normal error return, which callers already handle).
// Every public API in this header is byte-identical on both platforms;
// only Socket.cpp branches.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
// NOTE: winsock2.h must precede any windows.h inclusion.
#include <winsock2.h>
#endif

namespace cultulhu {
namespace net {

#ifdef _WIN32
// Winsock2 native handle. INVALID_SOCKET (~0) is the empty sentinel.
using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidNativeSocket = INVALID_SOCKET;
#else
using NativeSocket = int;
constexpr NativeSocket kInvalidNativeSocket = -1;
#endif

// Monotonic seconds, for tick timers (beacons, input/snapshot rates).
inline double nowSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// recvSome() return codes: >0 bytes read, 0 = peer closed, -1 = error,
// -2 = timed out waiting.
constexpr int kRecvTimeout = -2;

// Fire-and-forget UDP. Used for LAN/Radmin discovery beacons.
class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket() { close(); }
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    bool open();
    bool bind(uint16_t port);            // INADDR_ANY
    bool setBroadcast(bool on);
    bool sendTo(const std::string& ip, uint16_t port,
                const void* data, size_t len);
    // timeoutMs < 0 waits forever. Returns bytes, 0 on timeout, -1 on error.
    long recvFrom(void* buf, size_t cap, std::string& fromIp,
                  uint16_t& fromPort, int timeoutMs);
    void close();
    bool valid() const { return fd_ != kInvalidNativeSocket; }

private:
    NativeSocket fd_ = kInvalidNativeSocket;
};

// Connected TCP stream. Move-only (owns its fd).
class TcpSocket {
public:
    TcpSocket() = default;
    ~TcpSocket() { close(); }
    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;
    TcpSocket(TcpSocket&& o) noexcept;
    TcpSocket& operator=(TcpSocket&& o) noexcept;

    // Non-blocking connect with a select() timeout.
    bool connect(const std::string& ip, uint16_t port, int timeoutMs);
    bool sendAll(const void* data, size_t len);
    long recvSome(void* buf, size_t cap, int timeoutMs);
    void close();
    bool valid() const { return fd_ != kInvalidNativeSocket; }

    // For accepted sockets.
    static TcpSocket adopt(NativeSocket fd);

private:
    NativeSocket fd_ = kInvalidNativeSocket;
};

class TcpListener {
public:
    TcpListener() = default;
    ~TcpListener() { close(); }
    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;

    bool listen(uint16_t port, int backlog = 8);  // port 0 = ephemeral
    uint16_t boundPort() const;                   // actual port via getsockname
    std::optional<TcpSocket> accept(int timeoutMs);  // none on timeout
    void close();
    bool valid() const { return fd_ != kInvalidNativeSocket; }

private:
    NativeSocket fd_ = kInvalidNativeSocket;
};

} // namespace net
} // namespace cultulhu
