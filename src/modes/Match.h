#pragma once

// CULT-ULHU wave 21: the 5v5 match framework both multiplayer modes run on.
//
// A Match owns the roster (up to 10 players: humans + bots), the per-player
// simulation (movement, melee combat, deaths, respawns, KDA), and one of the
// two game modes (CapturePointMode or MobaDefense). The host simulates; the
// mode handles its own rules, fed occupancy / player-target data by Match.
//
// Player numbers (documented, tunable):
//   - 100 max HP, speed 5.0 u/s
//   - melee: range 3.0, 15 dps, 1.0 s swing cooldown
//   - respawn: capture mode 5 s; moba 5 s + 1 s per elapsed minute (cap 20 s)
//   - roster ids: 1000 + roster index (stable for PlayerKilled source/target)
//
// Achievement wiring (verified in tests_wave21):
//   - PlayerKilled published with sourceId=killer player id (0 when the
//     killer is not a player, e.g. a moba minion/tower), targetId=victim id,
//     faction=victim team -> first_blood / reaper fire on the real path.
//   - PointCaptured (capture mode) fires with faction=capturing team ->
//     standard_bearer.
//   - GreatOldOneSlain (moba) fires with sourceId=killing player id ->
//     godslayer; MatchEnded faction=winner -> dominion / unbroken.
//   - Turncoat: the ConversionPerformed handler in AchievementSystem was
//     verified (unit test: rival-faction conversion unlocks it). There is NO
//     conversion mechanic in 5v5 matches, so it is verified-but-
//     not-triggerable in 5v5. If a future mode adds conversions, publish
//     ConversionPerformed with faction = the turned soul's old faction.

#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "core/Vec3.h"
#include "modes/CapturePointMode.h"
#include "modes/MobaDefense.h"
#include "net/PlayerStats.h"
#include "net/Protocol.h"

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace cultulhu {

class Match {
public:
    static constexpr int kMaxPlayers = 10;
    static constexpr float kPlayerMaxHp = 100.0f;
    static constexpr float kPlayerSpeed = 5.0f;
    static constexpr float kMeleeRange = 3.0f;
    static constexpr float kMeleeDps = 15.0f;
    static constexpr double kSwingCooldown = 1.0;
    static constexpr double kCaptureRespawnSec = 5.0;
    static constexpr double kMobaRespawnBaseSec = 5.0;
    static constexpr double kMobaRespawnCapSec = 20.0;
    static constexpr uint64_t kPlayerIdBase = 1000;

    struct Player {
        uint64_t id = 0;
        std::string name;
        int team = 0; // 0 or 1
        Vec3 pos;
        float hp = kPlayerMaxHp;
        float maxHp = kPlayerMaxHp;
        bool alive = true;
        double respawnTimer = 0.0;
        bool isBot = true;
        double swingCd = 0.0;
        int lane = 0;        // moba: assigned lane (rosterIndex % 3)
        float laneFrac = 0;  // moba: how far along the lane path (0..1)
    };

    Match(EventBus& bus, GameClock& clock, RNG& rng);

    // mode: "capture" or "moba". Builds the mode, its default map, and
    // team spawn bases. Returns false for unknown modes.
    bool start(const std::string& mode);
    const std::string& modeName() const { return modeName_; }
    bool started() const { return mode_ != nullptr; }

    // team: 0/1; team < 0 auto-balances to the smaller team.
    uint64_t addPlayer(const std::string& name, int team);
    uint64_t addBot(int team); // name: "Bot <n>"
    // Fill both teams to 5 (bots only, humans keep their slots).
    void botfill();

    void update(double dt);
    bool isOver() const { return mode_ && mode_->isOver(); }
    int winner() const { return mode_ ? mode_->winner() : -1; }
    double elapsed() const { return elapsed_; }

    size_t playerCount() const { return players_.size(); }
    const Player& player(size_t i) const { return players_.at(i); }
    const Player* findPlayer(uint64_t id) const;

    CapturePointMode* capture() { return capture_; }
    MobaDefense* moba() { return moba_; }
    GameMode* mode() { return mode_.get(); }
    PlayerStatsTracker& stats() { return stats_; }

    // HostSnapshot-adjacent mode state for the wire (MsgType::ModeState).
    net::Message modeStateMessage() const;

    // Human-readable match dump for the driver's `match status`.
    void printStatus(std::ostream& os) const;

    // Test/GM utility: reposition a player (used by tests to stage fights).
    bool movePlayer(uint64_t id, Vec3 pos);

private:
    EventBus& bus_;
    GameClock& clock_;
    RNG& rng_;
    std::string modeName_;
    std::unique_ptr<GameMode> mode_; // owns the concrete mode below
    CapturePointMode* capture_ = nullptr;
    MobaDefense* moba_ = nullptr;
    std::vector<Player> players_;
    // No per-frame allocations: scratch buffers reserved once.
    std::vector<MobaDefense::PlayerTarget> playerTargets_;
    PlayerStatsTracker stats_;
    double elapsed_ = 0.0;
    int nextBotNum_ = 1;
    Vec3 teamBase_[2];

    int countTeam(int team) const;
    void spawnPlayer(Player& p);
    void updatePlayer(Player& p, size_t idx, double dt);
    void updateCaptureObjective(Player& p, double dt);
    void updateMobaObjective(Player& p, double dt);
    bool enemyInMeleeRange(const Player& p, size_t idx) const;
    bool attackNearestEnemy(Player& p, size_t idx, double dt);
    void swingAtStructures(Player& p);
    void damagePlayer(size_t victimIdx, float dmg);
    // killerIdx < 0 = non-player killer (entityId names it in the event).
    void killPlayer(size_t victimIdx, int killerIdx, uint64_t killerEntityId);
    void feedCaptureOccupants();
    void feedMobaPlayerTargets();
};

} // namespace cultulhu
