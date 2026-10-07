#include "net/Lobby.h"

#include "core/Logger.h"

namespace cultulhu {
namespace net {
namespace {

std::string cleanName(const std::string& n) {
    std::string out;
    for (char c : n) {
        if (c == ',' || c == ';' || c == '|' || c == '\n' || c == '\r') continue;
        out.push_back(c);
    }
    if (out.size() > 24) out.resize(24);
    return out.empty() ? "nameless" : out;
}

void sendMsg(TcpSocket& s, const Message& m) {
    auto bytes = encode(m);
    if (!s.sendAll(bytes.data(), bytes.size()))
        Logger::warn("Lobby: send failed");
}

} // namespace

std::string encodeLobbyPlayer(const LobbyPlayer& p) {
    return std::to_string(p.id) + "," + p.name + "," +
           (p.ready ? "1" : "0") + "," + std::to_string(p.team);
}

bool decodeLobbyPlayer(const std::string& s, LobbyPlayer& out) {
    std::vector<std::string> parts;
    size_t i = 0;
    // Split on ',' but the name may not contain ',' (cleanName strips it),
    // so a plain split is safe: id,name,ready,team.
    while (i <= s.size()) {
        size_t c = s.find(',', i);
        parts.push_back(s.substr(i, c == std::string::npos ? c : c - i));
        if (c == std::string::npos) break;
        i = c + 1;
    }
    if (parts.size() != 4) return false;
    try {
        out.id = static_cast<uint32_t>(std::stoul(parts[0]));
        out.name = parts[1];
        out.ready = parts[2] == "1";
        out.team = std::stoi(parts[3]);
    } catch (...) {
        return false;
    }
    return true;
}

// ---------------- HostLobby ----------------

HostLobby::HostLobby(uint16_t port, std::string hostName)
    : hostName_(cleanName(std::move(hostName))) {
    hostInfo_.id = 0;
    hostInfo_.name = hostName_;
    hostInfo_.team = 0;
    listenerPort_ = port;
}

bool HostLobby::start() {
    if (!listener_.listen(listenerPort_)) {
        Logger::error("HostLobby: cannot listen on TCP port");
        return false;
    }
    Logger::info("HostLobby: '" + hostName_ + "' listening on TCP " +
                 std::to_string(port()) + " (up to " +
                 std::to_string(kMaxLobbyPlayers) + " players)");
    return true;
}

void HostLobby::broadcast(const Message& m, const Slot* except) {
    for (auto& s : slots_) {
        if (except && s.get() == except) continue;
        sendMsg(s->sock, m);
    }
}

void HostLobby::broadcastPlayerList() {
    auto ps = players();
    Message m{MsgType::PlayerList, {}};
    m.fields["n"] = std::to_string(ps.size());
    for (size_t i = 0; i < ps.size(); ++i)
        m.fields["p" + std::to_string(i)] = encodeLobbyPlayer(ps[i]);
    broadcast(m);
}

std::vector<LobbyPlayer> HostLobby::players() const {
    std::vector<LobbyPlayer> out;
    out.push_back(hostInfo_);
    for (const auto& s : slots_)
        if (s->helloDone) out.push_back(s->info);
    return out;
}

int HostLobby::assignTeam() const {
    int t0 = 0, t1 = 0;
    for (const auto& s : slots_) {
        if (!s->helloDone) continue;
        if (s->info.team == 0) ++t0;
        else ++t1;
    }
    // Host counts as team 0.
    ++t0;
    if (t0 <= t1 && t0 <= kTeamSize) return 0;
    if (t1 < kTeamSize) return 1;
    return 0;
}

void HostLobby::removeSlot(size_t i, const std::string& reason) {
    if (i >= slots_.size()) return;
    Logger::info("HostLobby: '" + slots_[i]->info.name + "' left (" + reason +
                 ")");
    slots_.erase(slots_.begin() + i);
    broadcastPlayerList();
}

void HostLobby::poll() {
    // Accept new connections.
    for (;;) {
        auto acc = listener_.accept(0);
        if (!acc) break;
        if (static_cast<int>(slots_.size()) >= kMaxLobbyPlayers - 1) {
            Logger::warn("HostLobby: lobby full, rejecting connection");
            continue;  // socket closes on destruction
        }
        auto slot = std::make_unique<Slot>();
        slot->sock = std::move(*acc);
        slot->info.id = nextId_++;
        slot->info.team = assignTeam();
        slots_.push_back(std::move(slot));
        Logger::info("HostLobby: incoming connection (awaiting Hello)");
    }

    // Pump existing clients.
    uint8_t buf[4096];
    for (size_t i = 0; i < slots_.size();) {
        Slot& s = *slots_[i];
        long n = s.sock.recvSome(buf, sizeof(buf), 0);
        if (n == 0 || n == -1) {
            removeSlot(i, n == 0 ? "disconnect" : "error");
            continue;
        }
        if (n > 0) {
            s.reader.feed(buf, static_cast<size_t>(n));
            for (const Message& m : s.reader.drain()) {
                if (!s.helloDone) {
                    if (m.type == MsgType::Hello &&
                        fieldInt(m, "version", 0) == kProtocolVersion) {
                        s.info.name = cleanName(fieldStr(m, "name"));
                        s.helloDone = true;
                        Message w{MsgType::Welcome, {}};
                        w.fields["id"] = std::to_string(s.info.id);
                        w.fields["name"] = s.info.name;
                        sendMsg(s.sock, w);
                        Logger::info("HostLobby: '" + s.info.name +
                                     "' joined as player " +
                                     std::to_string(s.info.id) + " (team " +
                                     std::to_string(s.info.team) + ")");
                        broadcastPlayerList();
                    } else {
                        removeSlot(i, "bad hello");
                        goto next_slot;
                    }
                } else if (m.type == MsgType::Ready) {
                    s.info.ready = fieldInt(m, "ready", 0) != 0;
                    broadcastPlayerList();
                } else if (m.type == MsgType::ChatMsg) {
                    std::string line = s.info.name + ": " +
                                       fieldStr(m, "text");
                    chatLog_.push_back(line);
                    Message out{MsgType::ChatMsg, {}};
                    out.fields["from"] = s.info.name;
                    out.fields["text"] = fieldStr(m, "text");
                    broadcast(out, &s);  // relay to everyone else
                } else if (m.type == MsgType::Disconnect) {
                    removeSlot(i, fieldStr(m, "reason", "bye"));
                    goto next_slot;
                }
            }
        }
        ++i;
    next_slot:;
    }
}

void HostLobby::setHostReady(bool r) {
    hostInfo_.ready = r;
    broadcastPlayerList();
}

bool HostLobby::allReady() const {
    if (!hostInfo_.ready) return false;
    for (const auto& s : slots_) {
        if (!s->helloDone || !s->info.ready) return false;
    }
    return !slots_.empty() || hostInfo_.ready;
}

bool HostLobby::startGame(bool force, const std::string& mode) {
    if (started_) return true;
    if (!force && !allReady()) {
        Logger::warn("HostLobby: not everyone is ready (use force to start)");
        return false;
    }
    started_ = true;
    Message m{MsgType::StartGame, {}};
    m.fields["mode"] = mode;
    m.fields["seed"] = std::to_string(12345);  // v1: fixed seed
    broadcast(m);
    Logger::info("HostLobby: game starting (" + mode + ")");
    return true;
}

void HostLobby::sendChatAll(const std::string& from, const std::string& text) {
    chatLog_.push_back(from + ": " + text);
    Message m{MsgType::ChatMsg, {}};
    m.fields["from"] = from;
    m.fields["text"] = text;
    broadcast(m);
}

std::vector<std::string> HostLobby::drainChat() {
    auto out = chatLog_;
    chatLog_.clear();
    return out;
}

std::vector<TcpSocket> HostLobby::takeClientSockets() {
    std::vector<TcpSocket> out;
    if (socketsTaken_) return out;
    socketsTaken_ = true;
    for (auto& s : slots_) out.push_back(std::move(s->sock));
    return out;
}

// ---------------- JoinLobby ----------------

bool JoinLobby::connect(const std::string& ip, uint16_t port,
                        const std::string& name) {
    name_ = cleanName(name);
    if (!sock_.connect(ip, port, 5000)) {
        Logger::error("JoinLobby: cannot connect to " + ip + ":" +
                      std::to_string(port));
        return false;
    }
    Message h{MsgType::Hello, {}};
    h.fields["name"] = name_;
    h.fields["version"] = std::to_string(kProtocolVersion);
    sendMsg(sock_, h);
    Logger::info("JoinLobby: connected, sent Hello as '" + name_ + "'");
    return true;
}

void JoinLobby::poll() {
    if (!sock_.valid() || socketTaken_) return;
    uint8_t buf[4096];
    long n = sock_.recvSome(buf, sizeof(buf), 0);
    if (n == 0 || n == -1) {
        Logger::warn("JoinLobby: lost connection to host");
        sock_.close();
        return;
    }
    if (n < 0) return;  // timeout
    reader_.feed(buf, static_cast<size_t>(n));
    for (const Message& m : reader_.drain()) {
        switch (m.type) {
            case MsgType::Welcome:
                myId_ = static_cast<uint32_t>(fieldInt(m, "id", 0));
                welcomed_ = true;
                Logger::info("JoinLobby: welcomed as player " +
                             std::to_string(myId_));
                break;
            case MsgType::PlayerList: {
                players_.clear();
                int n = fieldInt(m, "n", 0);
                for (int i = 0; i < n; ++i) {
                    LobbyPlayer p;
                    if (decodeLobbyPlayer(fieldStr(m, "p" + std::to_string(i)),
                                          p))
                        players_.push_back(p);
                }
                break;
            }
            case MsgType::ChatMsg:
                chatLog_.push_back(fieldStr(m, "from") + ": " +
                                   fieldStr(m, "text"));
                break;
            case MsgType::StartGame:
                started_ = true;
                startMode_ = fieldStr(m, "mode", "freeroam");
                startSeed_ =
                    static_cast<uint32_t>(fieldInt(m, "seed", 0));
                Logger::info("JoinLobby: game starting (" + startMode_ + ")");
                break;
            case MsgType::Disconnect:
                Logger::warn("JoinLobby: kicked (" +
                             fieldStr(m, "reason", "?") + ")");
                sock_.close();
                break;
            default:
                break;
        }
    }
}

void JoinLobby::setReady(bool r) {
    ready_ = r;
    if (!sock_.valid() || !welcomed_) return;
    Message m{MsgType::Ready, {}};
    m.fields["id"] = std::to_string(myId_);
    m.fields["ready"] = r ? "1" : "0";
    sendMsg(sock_, m);
}

void JoinLobby::sendChat(const std::string& text) {
    if (!sock_.valid()) return;
    Message m{MsgType::ChatMsg, {}};
    m.fields["from"] = name_;
    m.fields["text"] = text;
    sendMsg(sock_, m);
}

void JoinLobby::disconnect(const std::string& reason) {
    if (sock_.valid()) {
        Message m{MsgType::Disconnect, {}};
        m.fields["id"] = std::to_string(myId_);
        m.fields["reason"] = reason;
        sendMsg(sock_, m);
    }
    sock_.close();
}

std::vector<std::string> JoinLobby::drainChat() {
    auto out = chatLog_;
    chatLog_.clear();
    return out;
}

TcpSocket JoinLobby::takeSocket() {
    socketTaken_ = true;
    return std::move(sock_);
}

} // namespace net
} // namespace cultulhu
