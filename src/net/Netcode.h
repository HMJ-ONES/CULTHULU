#pragma once

// Host-authoritative gameplay netcode for CULT-ULHU (v1, pragmatic).
//
// Model: the host's PC runs the real simulation. Clients send compact
// ClientInput at ~30 Hz; the host broadcasts HostSnapshot at ~20 Hz with
// the positions/hp/state of relevant entities; clients apply snapshots
// directly.
//
// v1 limitations (documented, not bugs):
//   - No client-side prediction or reconciliation; on high latency the
//     client's view lags the host by ~1 snapshot interval. Fine for
//     co-op cult mayhem over a virtual LAN, where RTT is usually <50ms.
//   - No delta compression or interest management; snapshots carry every
//     tracked entity. Fine at our entity counts; revisit past ~200.
//   - No encryption; Radmin VPN already encrypts the tunnel.

#include "net/Protocol.h"
#include "net/Socket.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace cultulhu {
namespace net {

constexpr double kInputHz = 30.0;
constexpr double kSnapshotHz = 20.0;

// buttons bitmask: bit0 = attack, bit1 = cast, bit2 = jump/interact.
struct ClientInput {
    uint32_t seq = 0;
    float moveX = 0.0f;  // -1..1
    float moveZ = 0.0f;  // -1..1
    float yaw = 0.0f;    // radians
    uint8_t buttons = 0;
};

struct SnapshotEntity {
    uint32_t id = 0;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float hp = 0.0f;
    uint8_t state = 0;  // opaque entity-state byte (anim/AI state)
};

Message encodeClientInput(const ClientInput& in);
bool decodeClientInput(const Message& m, ClientInput& out);
Message encodeSnapshot(uint32_t tick,
                       const std::vector<SnapshotEntity>& ents);
bool decodeSnapshot(const Message& m, uint32_t& tickOut,
                    std::vector<SnapshotEntity>& entsOut);

// Host side: owns the lobby's client sockets after StartGame.
class NetHost {
public:
    explicit NetHost(std::vector<TcpSocket> clients);

    using SnapshotProvider = std::function<std::vector<SnapshotEntity>()>;

    // Broadcasts snapshots at kSnapshotHz; collects client inputs.
    // Call often (every driver tick). Dead clients are dropped.
    void poll(double nowSeconds, SnapshotProvider provide);

    // Latest input per client index (matches takeClientSockets() order).
    // Absent entry = no input received yet.
    const std::map<size_t, ClientInput>& latestInputs() const {
        return inputs_;
    }
    size_t clientCount() const { return clients_.size(); }

private:
    struct Client {
        TcpSocket sock;
        MessageReader reader;
        bool alive = true;
    };
    std::vector<Client> clients_;
    std::map<size_t, ClientInput> inputs_;
    uint32_t tick_ = 0;
    double lastSnap_ = -1e9;
};

// Client side: owns the lobby socket after StartGame.
class NetClient {
public:
    explicit NetClient(TcpSocket sock);

    using InputProvider = std::function<ClientInput()>;

    // Sends input at kInputHz; applies incoming snapshots directly.
    void poll(double nowSeconds, InputProvider provide);

    const std::map<uint32_t, SnapshotEntity>& entities() const {
        return entities_;
    }
    uint32_t lastTick() const { return lastTick_; }
    bool connected() const { return alive_; }

private:
    TcpSocket sock_;
    MessageReader reader_;
    std::map<uint32_t, SnapshotEntity> entities_;
    uint32_t lastTick_ = 0;
    uint32_t seq_ = 0;
    double lastInput_ = -1e9;
    bool alive_ = true;
};

} // namespace net
} // namespace cultulhu
