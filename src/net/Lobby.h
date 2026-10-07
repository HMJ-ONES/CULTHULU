#pragma once

// Pre-game lobby for CULT-ULHU multiplayer.
//
// Host (the player whose PC runs the match):
//   HostLobby listens on TCP, accepts up to 10 players, tracks names and
//   ready states, auto-assigns two teams of five, and starts the game.
// Client:
//   JoinLobby connects by IP:port, registers a name, toggles ready, and
//   follows lobby updates until the host starts.
//
// Both are poll-driven (no threads): call poll() often from the driver loop.
// After StartGame, hand the sockets to NetHost / NetClient (see Netcode.h).

#include "net/Protocol.h"
#include "net/Socket.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace cultulhu {
namespace net {

constexpr int kMaxLobbyPlayers = 10;  // 5v5
constexpr int kTeamSize = 5;

struct LobbyPlayer {
    uint32_t id = 0;
    std::string name;
    bool ready = false;
    int team = -1;  // 0 or 1
};

// "1,Alice,1,0" helpers shared by both sides.
std::string encodeLobbyPlayer(const LobbyPlayer& p);
bool decodeLobbyPlayer(const std::string& s, LobbyPlayer& out);

class HostLobby {
public:
    HostLobby(uint16_t port, std::string hostName);
    bool start();
    uint16_t port() const { return listener_.boundPort(); }

    void poll();  // accept new + pump all client sockets (non-blocking)

    std::vector<LobbyPlayer> players() const;  // host (id 0) first
    void setHostReady(bool r);
    bool allReady() const;
    // Broadcasts StartGame; returns false unless allReady() or force.
    bool startGame(bool force, const std::string& mode = "freeroam");
    bool gameStarted() const { return started_; }

    void sendChatAll(const std::string& from, const std::string& text);
    std::vector<std::string> drainChat();

    // Hand accepted sockets to the gameplay netcode. Lobby stays usable
    // for chat/updates only until game start; call once.
    std::vector<TcpSocket> takeClientSockets();

private:
    struct Slot {
        TcpSocket sock;
        MessageReader reader;
        LobbyPlayer info;
        bool helloDone = false;
    };

    void broadcast(const Message& m, const Slot* except = nullptr);
    void broadcastPlayerList();
    void removeSlot(size_t i, const std::string& reason);
    int assignTeam() const;

    TcpListener listener_;
    uint16_t listenerPort_ = 0;
    std::string hostName_;
    LobbyPlayer hostInfo_;  // id 0
    std::vector<std::unique_ptr<Slot>> slots_;
    std::vector<std::string> chatLog_;
    uint32_t nextId_ = 1;
    bool started_ = false;
    bool socketsTaken_ = false;
};

class JoinLobby {
public:
    bool connect(const std::string& ip, uint16_t port,
                 const std::string& name);
    void poll();  // non-blocking pump
    void setReady(bool r);
    void sendChat(const std::string& text);
    void disconnect(const std::string& reason = "bye");

    std::vector<LobbyPlayer> players() const { return players_; }
    std::vector<std::string> drainChat();
    bool gameStarted() const { return started_; }
    std::string startMode() const { return startMode_; }
    uint32_t startSeed() const { return startSeed_; }
    bool connected() const { return sock_.valid(); }
    uint32_t myId() const { return myId_; }
    std::string myName() const { return name_; }

    // Hand the socket to NetClient after StartGame. Call once.
    TcpSocket takeSocket();

private:
    TcpSocket sock_;
    MessageReader reader_;
    std::string name_;
    uint32_t myId_ = 0;
    bool welcomed_ = false;
    bool ready_ = false;
    std::vector<LobbyPlayer> players_;
    std::vector<std::string> chatLog_;
    bool started_ = false;
    std::string startMode_;
    uint32_t startSeed_ = 0;
    bool socketTaken_ = false;
};

} // namespace net
} // namespace cultulhu
