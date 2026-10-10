#include "net/Netcode.h"

#include "core/Logger.h"

#include <cstdio>

namespace cultulhu {
namespace net {

Message encodeClientInput(const ClientInput& in) {
    Message m{MsgType::ClientInput, {}};
    m.fields["seq"] = std::to_string(in.seq);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.3f", in.moveX);
    m.fields["mx"] = buf;
    std::snprintf(buf, sizeof(buf), "%.3f", in.moveZ);
    m.fields["mz"] = buf;
    std::snprintf(buf, sizeof(buf), "%.3f", in.yaw);
    m.fields["yaw"] = buf;
    m.fields["btn"] = std::to_string(static_cast<int>(in.buttons));
    return m;
}

bool decodeClientInput(const Message& m, ClientInput& out) {
    if (m.type != MsgType::ClientInput) return false;
    out.seq = static_cast<uint32_t>(fieldInt(m, "seq", 0));
    out.moveX = fieldFloat(m, "mx", 0.0f);
    out.moveZ = fieldFloat(m, "mz", 0.0f);
    out.yaw = fieldFloat(m, "yaw", 0.0f);
    out.buttons = static_cast<uint8_t>(fieldInt(m, "btn", 0));
    return true;
}

Message encodeSnapshot(uint32_t tick,
                       const std::vector<SnapshotEntity>& ents) {
    Message m{MsgType::HostSnapshot, {}};
    m.fields["tick"] = std::to_string(tick);
    m.fields["n"] = std::to_string(ents.size());
    char buf[128];
    for (size_t i = 0; i < ents.size(); ++i) {
        const auto& e = ents[i];
        std::snprintf(buf, sizeof(buf), "%u,%.2f,%.2f,%.2f,%.1f,%u", e.id,
                      e.x, e.y, e.z, e.hp,
                      static_cast<unsigned>(e.state));
        m.fields["e" + std::to_string(i)] = buf;
    }
    return m;
}

bool decodeSnapshot(const Message& m, uint32_t& tickOut,
                    std::vector<SnapshotEntity>& entsOut) {
    if (m.type != MsgType::HostSnapshot) return false;
    tickOut = static_cast<uint32_t>(fieldInt(m, "tick", 0));
    int n = fieldInt(m, "n", 0);
    // Wave 9d: 'n' is remote-controlled. Cap it: without a bound a
    // malicious/buggy host could force billions of loop iterations here
    // (CPU hang). Legitimate snapshots carry a handful of entities.
    if (n < 0 || n > 4096) return false;
    entsOut.clear();
    for (int i = 0; i < n; ++i) {
        std::string s = fieldStr(m, "e" + std::to_string(i));
        SnapshotEntity e;
        // id,x,y,z,hp,state — parse the five floats/ints, then the
        // trailing state byte separately for robustness.
        if (std::sscanf(s.c_str(), "%u,%f,%f,%f,%f", &e.id, &e.x, &e.y,
                        &e.z, &e.hp) < 5)
            continue;
        size_t last = s.rfind(',');
        if (last != std::string::npos) {
            try {
                e.state = static_cast<uint8_t>(std::stoi(s.substr(last + 1)));
            } catch (...) {
                e.state = 0;
            }
        }
        entsOut.push_back(e);
    }
    return true;
}

// ---------------- NetHost ----------------

NetHost::NetHost(std::vector<TcpSocket> clients) {
    for (auto& s : clients) {
        Client c;
        c.sock = std::move(s);
        clients_.push_back(std::move(c));
    }
    Logger::info("NetHost: gameplay live with " +
                 std::to_string(clients_.size()) + " clients");
}

void NetHost::poll(double now, SnapshotProvider provide) {
    uint8_t buf[8192];
    // 1) Collect inputs.
    for (size_t i = 0; i < clients_.size(); ++i) {
        Client& c = clients_[i];
        if (!c.alive) continue;
        long n = c.sock.recvSome(buf, sizeof(buf), 0);
        if (n == 0 || n == -1) {
            c.alive = false;
            Logger::warn("NetHost: client " + std::to_string(i) +
                         " disconnected");
            continue;
        }
        if (n > 0) {
            c.reader.feed(buf, static_cast<size_t>(n));
            for (const Message& m : c.reader.drain()) {
                if (m.type == MsgType::ClientInput) {
                    ClientInput in;
                    if (decodeClientInput(m, in)) inputs_[i] = in;
                } else if (m.type == MsgType::Disconnect) {
                    c.alive = false;
                }
            }
        }
    }
    // 2) Broadcast snapshot at kSnapshotHz.
    if (now - lastSnap_ >= 1.0 / kSnapshotHz) {
        lastSnap_ = now;
        ++tick_;
        auto bytes = encode(encodeSnapshot(tick_, provide()));
        for (size_t i = 0; i < clients_.size(); ++i) {
            Client& c = clients_[i];
            if (!c.alive) continue;
            if (!c.sock.sendAll(bytes.data(), bytes.size())) {
                c.alive = false;
                Logger::warn("NetHost: send to client " +
                             std::to_string(i) + " failed");
            }
        }
    }
    // 3) Broadcast KDA standings at kKdaHz when the game provided one.
    if (kdaProvider_ && now - lastKda_ >= 1.0 / kKdaHz) {
        lastKda_ = now;
        auto bytes = encode(encodePlayerKda(kdaProvider_()));
        for (size_t i = 0; i < clients_.size(); ++i) {
            Client& c = clients_[i];
            if (!c.alive) continue;
            if (!c.sock.sendAll(bytes.data(), bytes.size())) {
                c.alive = false;
                Logger::warn("NetHost: kda send to client " +
                             std::to_string(i) + " failed");
            }
        }
    }
}

// ---------------- NetClient ----------------

NetClient::NetClient(TcpSocket sock) : sock_(std::move(sock)) {
    Logger::info("NetClient: gameplay live");
}

void NetClient::poll(double now, InputProvider provide) {
    if (!alive_) return;
    // 1) Send input at kInputHz.
    if (now - lastInput_ >= 1.0 / kInputHz) {
        lastInput_ = now;
        ClientInput in = provide();
        in.seq = ++seq_;
        auto bytes = encode(encodeClientInput(in));
        if (!sock_.sendAll(bytes.data(), bytes.size())) {
            alive_ = false;
            Logger::warn("NetClient: lost connection to host");
            return;
        }
    }
    // 2) Apply snapshots directly (no prediction in v1).
    uint8_t buf[65536];
    long n = sock_.recvSome(buf, sizeof(buf), 0);
    if (n == 0 || n == -1) {
        alive_ = false;
        Logger::warn("NetClient: host disconnected");
        return;
    }
    if (n > 0) {
        reader_.feed(buf, static_cast<size_t>(n));
        for (const Message& m : reader_.drain()) {
            if (m.type == MsgType::HostSnapshot) {
                uint32_t tick;
                std::vector<SnapshotEntity> ents;
                if (decodeSnapshot(m, tick, ents) && tick >= lastTick_) {
                    lastTick_ = tick;
                    for (const auto& e : ents) entities_[e.id] = e;
                }
            } else if (m.type == MsgType::Disconnect) {
                alive_ = false;
            } else if (m.type == MsgType::PlayerKda) {
                std::vector<KdaEntry> rows;
                if (decodePlayerKda(m, rows)) kda_ = std::move(rows);
            }
        }
    }
}

} // namespace net
} // namespace cultulhu
