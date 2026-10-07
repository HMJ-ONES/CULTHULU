#pragma once

// Lightweight POSIX socket wrappers for CULT-ULHU multiplayer.
// Engine-agnostic; Linux first.
//
// PORTING NOTE (Windows): replace the POSIX headers in Socket.cpp with
// Winsock2 (<winsock2.h>, <ws2tcpip.h>): call WSAStartup once at startup,
// use SOCKET instead of int fd (INVALID_SOCKET sentinel), closesocket()
// instead of ::close(), ioctlsocket(FIONBIO) instead of fcntl(O_NONBLOCK).
// Every public API in this header is designed to stay byte-identical; only
// the .cpp needs a Win32 branch.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cultulhu {
namespace net {

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
    bool valid() const { return fd_ >= 0; }

private:
    int fd_ = -1;
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
    bool valid() const { return fd_ >= 0; }

    // For accepted sockets.
    static TcpSocket adopt(int fd);

private:
    int fd_ = -1;
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
    bool valid() const { return fd_ >= 0; }

private:
    int fd_ = -1;
};

} // namespace net
} // namespace cultulhu
