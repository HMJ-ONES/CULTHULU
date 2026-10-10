#pragma once

// CULT-ULHU multiplayer wire protocol (v1).
//
// Framing:  [type: u8][length: u32 big-endian][payload: length bytes]
// Payload:  plain text "key=value;key=value;" pairs (UTF-8, no ';' or '='
//            allowed inside values — chat text is sanitized on encode).
// Text was chosen over packed binary so packets are human-readable in
// logs and trivially debuggable with netcat. Bandwidth is not a concern
// at 10 players and 20-30 Hz with these tiny messages.
//
// Message types:
//   Hello        C->H  {name, version}            join request
//   Welcome      H->C  {id, name}                 assigned player id
//   PlayerList   H->*  {n, p0, p1, ...}           roster broadcast;
//                                   each pK = "id,name,ready,team"
//   ChatMsg      *->*  {from, text}               relayed by host
//   Ready        C->H  {id, ready}                ready toggle
//   StartGame    H->*  {mode, seed}               lobby -> game
//   ClientInput  C->H  {seq, mx, mz, yaw, btn}    30 Hz
//   HostSnapshot H->C  {tick, n, e0, e1, ...}     20 Hz;
//                                   each eK = "id,x,y,z,hp,state"
//   Disconnect   *->*  {id, reason}
//   PlayerKda    H->C  {n, e0, e1, ...}           ~1 Hz KDA standings;
//                                   each eK = "playerIdx,kills,deaths,assists,name"
//   ModeState    H->C  {mode, ...}                ~1 Hz match mode state;
//                                   capture: scores + per-point owner/progress
//                                   moba: base HPs + tower HPs + minion count

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "net/Kda.h"

namespace cultulhu {
namespace net {

enum class MsgType : uint8_t {
    Hello = 0,
    Welcome = 1,
    PlayerList = 2,
    ChatMsg = 3,
    Ready = 4,
    StartGame = 5,
    ClientInput = 6,
    HostSnapshot = 7,
    Disconnect = 8,
    PlayerKda = 9,
    ModeState = 10,
};

constexpr int kProtocolVersion = 1;

struct Message {
    MsgType type;
    std::map<std::string, std::string> fields;
};

// "key=value;key=value;" — deterministic order (std::map), ';' stripped.
std::string encodeFields(const std::map<std::string, std::string>& fields);
std::map<std::string, std::string> decodeFields(const std::string& payload);

// One framed packet.
std::vector<uint8_t> encode(const Message& m);
std::optional<Message> decodeFrame(const uint8_t* data, size_t len);

// Incremental parser for TCP streams: feed arbitrary chunks, drain
// returns every complete message parsed so far.
class MessageReader {
public:
    void feed(const uint8_t* data, size_t len);
    std::vector<Message> drain();
    void clear() { buf_.clear(); }

private:
    std::vector<uint8_t> buf_;
};

// Typed field helpers (missing/bad values -> default).
std::string fieldStr(const Message& m, const std::string& key,
                     const std::string& dflt = "");
int fieldInt(const Message& m, const std::string& key, int dflt = 0);
float fieldFloat(const Message& m, const std::string& key, float dflt = 0.0f);

// One KDA standings row, tagged with the player index it belongs to
// (names alone are not stable keys).
struct KdaEntry {
    uint32_t playerIdx = 0;
    KdaRow row;
};

// Host -> clients standings broadcast.
Message encodePlayerKda(const std::vector<KdaEntry>& entries);
bool decodePlayerKda(const Message& m, std::vector<KdaEntry>& out);

// Host -> clients match mode state (wave 21). Compact view of the active
// 5v5 mode for clients: mode tag ("capture"/"moba"), scores, and
// mode-specific details. The host's NetHost::poll should broadcast one at
// ~1 Hz whenever a match is active (see Netcode.h hook note).
struct ModeState {
    std::string mode = "none"; // "capture" | "moba" | "none"
    // Capture: scores; per-point owner (-1 neutral) + capture progress.
    float score0 = 0.0f, score1 = 0.0f;
    std::vector<int> pointOwners;
    std::vector<float> pointProg0, pointProg1;
    // MOBA: base HPs, flat tower HP list (team 0/1 x lane x idx), live
    // minion count.
    float baseHp0 = 0.0f, baseHp1 = 0.0f;
    std::vector<float> towerHps;
    int minionCount = 0;
};

Message encodeModeState(const ModeState& s);
bool decodeModeState(const Message& m, ModeState& out);

} // namespace net
} // namespace cultulhu
