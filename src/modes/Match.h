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

#include "characters/CharacterDef.h"
#include "characters/CharacterRegistry.h"
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
    // Waveclear: players (bots and humans) hit minions much harder than
    // they hit players/structures. This is the push engine — clearing the
    // enemy wave lets your own wave reach the tower healthy.
    static constexpr float kWaveclearDps = 30.0f;
    static constexpr double kSwingCooldown = 1.0;
    static constexpr double kCaptureRespawnSec = 5.0;
    static constexpr double kMobaRespawnBaseSec = 5.0;
    static constexpr double kMobaRespawnCapSec = 20.0;
    // Wave 33: base invocation — 20s channel within 15m of your base.
    static constexpr float kBaseInvokeRadius = 15.0f;
    static constexpr double kInvokeChannelSec = 20.0;
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
        // Wave 42: AFK takeover. When the human goes AFK, a bot pilots
        // their fighter (isBot stays false — this is still their slot).
        // Match continues, no pausing. Rewards depend on participation.
        bool afk = false;
        double afkSince = 0.0;    // match time when AFK started
        double afkSeconds = 0.0;   // total AFK time this match
        double swingCd = 0.0;
        int lane = 0;        // moba: assigned lane (rosterIndex % 3)
        float laneFrac = 0;  // moba: how far along the lane path (0..1)
        float facingYaw = 0.0f; // camera boom / FP look reference
        // Wave 33: MOBA relics — damage multiplier from held artifacts
        // (same pool as free-roam), capped like free-roam's 3x.
        float dmgMult = 1.0f;
        std::vector<std::string> relicNames;
        // Wave 33/34: levels and base invocation. XP from kills, minion
        // kills, and tower takedowns; level = 1 + xp/100. Each level
        // grants one invoke charge: spend 20s channeling at your base to
        // call down an artifact, even one never found in the jungle.
        // Wave 34: artifacts have tiers — invoke tier 1 at 10+, tier 2
        // at 20+, late-game tier 3 at 30. No money, no shop.
        int xp = 0;
        int level = 1;
        int pendingInvokes = 0;
        double invokeTimer = 0.0;
        bool invoking = false;
        // Wave 34: cooldown between invoke trips (prevents yo-yoing).
        double lastInvokeTrip = -1000.0;
        // Wave 40: directional kit combat. Every fighter wields a full
        // character kit (Q/F/R + RMB) — the same kits as free-roam.
        std::string kitId; // character id, e.g. "cthulhu_avatar"
        double qCd = 0.0, fCd = 0.0, rCd = 0.0, rmbCd = 0.0;
        double ccTimer = 0.0;   // stunned/rooted: cannot act
        double fearTimer = 0.0; // fleeing from fearFrom
        Vec3 fearFrom{0, 0, 0};
        double buffTimer = 0.0;  // +30% damage while active
        double shieldTimer = 0.0; // half damage taken while active
        double slowTimer = 0.0;   // half move speed while active
        // Wave 41: ability unlocks.
        // Domination: everything unlocked, but the ult (R) starts on its
        // full cooldown — 0% charged at match start.
        // DOTGOO: draft your kit as you level. Level 1: pick Q or F.
        // Level 3: pick the other. Level 6: the ult (R) unlocks.
        bool qUnlocked = false, fUnlocked = false, rUnlocked = false;
        int pendingPicks = 0; // DOTGOO: unspent ability draft choices
        // Wave 42: Cthulhu's rock (Domination only — no minions to dominate).
        // RMB aimed at the ground rips a chunk of rock: frontal shield
        // (half damage, soaks 6 melee-hits) + heavy swings (1.0 each) +
        // throwable (damage scales with remaining integrity). Mixed use
        // works: swing twice then hurl, or block then hurl for the bonus.
        bool hasRock = false;
        float rockDurability = 0.0f; // 3.0 on rip; breaks at 0
        // Wave 42: kill streaks (fun). Resets if 8s pass without a kill.
        int killStreak = 0;
        double lastKillAt = -1000.0;
    };

    // Wave 42: how the RMB is aimed — Normal (at enemies), Down (at the
    // ground, Cthulhu rips his rock), Launch (hurl the held rock).
    enum class RmbAim { Normal, Down, Launch };

    Match(EventBus& bus, GameClock& clock, RNG& rng);

    // mode: "capture" or "moba". Builds the mode, its default map, and
    // team spawn bases. Returns false for unknown modes.
    bool start(const std::string& mode);
    const std::string& modeName() const { return modeName_; }
    bool started() const { return mode_ != nullptr; }

    // team: 0/1; team < 0 auto-balances to the smaller team.
    uint64_t addPlayer(const std::string& name, int team);
    uint64_t addBot(int team); // name: "Bot <n>"
    // Take over a bot slot on team as the local human ("You"). Returns the
    // player id, or 0 when no bot on that team can be replaced.
    uint64_t joinAsHuman(int team);
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
    const CapturePointMode* capture() const { return capture_; }
    const MobaDefense* moba() const { return moba_; }
    GameMode* mode() { return mode_.get(); }
    PlayerStatsTracker& stats() { return stats_; }
    const PlayerStatsTracker& stats() const { return stats_; }
    EventBus& bus() { return bus_; } // wave 42: for MatchLogger

    // HostSnapshot-adjacent mode state for the wire (MsgType::ModeState).
    net::Message modeStateMessage() const;

    // Human-readable match dump for the driver's `match status`.
    // humanId marks the local human's roster row with " (YOU)" so they can
    // spot their own fighter among the bots; 0 = no marker.
    void printStatus(std::ostream& os) const;
    void printStatus(std::ostream& os, uint64_t humanId) const;

    // Test/GM utility: reposition a player (used by tests to stage fights).
    bool movePlayer(uint64_t id, Vec3 pos);
    // Test/GM utility: grant XP directly (used by tests to stage
    // level-ups without playing out full fights).
    bool grantXp(uint64_t id, int amount);
    // Wave 40: assign a specific kit (used by tests for deterministic fights).
    bool setPlayerKit(uint64_t id, const std::string& kitId);
    // Wave 42: AFK takeover — bot pilots the human's fighter while away.
    void setPlayerAfk(uint64_t id, bool afk);
    bool isPlayerAfk(uint64_t id) const;
    // Wave 42: fraction of the match the player spent AFK (0..1).
    // >= 0.95 means AFK the whole match -> no rewards.
    double afkFraction(uint64_t id) const;
    // Human camera control: set a player's facing yaw.
    bool setFacingYaw(uint64_t id, float yaw);
    // Wave 40: human kit casting in matches. slot 0=Q 1=F 2=R 3=RMB.
    // Faces `yaw` first so casts are directional, like the bots'.
    bool humanCast(uint64_t id, int slot, float yaw, RmbAim aim = RmbAim::Normal);
    // Wave 41: spend a pending draft pick (DOTGOO). slot 0=Q 1=F.
    bool draftAbility(uint64_t id, int slot);

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
    // Micro-dev (2026-10-12): the match's first death gets an arena
    // callout (EventType::FirstBlood) — once per match, whatever the
    // cause. Reset in start().
    bool firstBloodDone_ = false;
    int nextBotNum_ = 1;
    Vec3 teamBase_[2];

    int countTeam(int team) const;
    void spawnPlayer(Player& p);
    void updatePlayer(Player& p, size_t idx, double dt);
    void updateCaptureObjective(Player& p, double dt);
    void updateMobaObjective(Player& p, double dt);
    // Furthest-advanced live friendly minion in (team, lane); bot escort anchor.
    bool furthestFriendlyMinion(int team, int lane, Vec3& out) const;
    bool enemyInMeleeRange(const Player& p, size_t idx) const;
    bool attackNearestEnemy(Player& p, size_t idx, double dt);
    void swingAtStructures(Player& p);
    void damagePlayer(size_t victimIdx, float dmg);
    // Wave 42 fix: melee auto-attacks bypassed the rock shield because
    // this path never saw the attacker. The overload takes the
    // attacker's position so the frontal shield can face it. (Minion/
    // tower damage keeps the old path — rocks don't exist in DOTGOO.)
    void damagePlayer(size_t victimIdx, float dmg, const Vec3& attackerPos);
    // killerIdx < 0 = non-player killer (entityId names it in the event).
    void killPlayer(size_t victimIdx, int killerIdx, uint64_t killerEntityId);
    // Wave 34: XP economy. Minion kills 60, tower destructions 800,
    // player kills scale with victim level (50 + 25*lvl). Level = 1 + xp/50.
    // every level-up grants one invoke charge.
    void addXp(size_t playerIdx, int amount);
    void feedCaptureOccupants();
    void feedMobaPlayerTargets();
    // Wave 40: directional kit combat (defined in MatchKits.cpp).
    static constexpr float kKitDmgScale = 0.2f; // free-roam power -> match scale
    const CharacterDef* kitOf(const Player& p) const;
    void assignKit(Player& p);
    // Wave 41: set initial ability unlocks per mode.
    void initAbilityUnlocks(Player& p);
    float kitDamage(const Player& p, float effectPower) const;
    int enemyInCone(const Player& p, size_t idx, float range,
                    float halfAngleDeg) const;
    int enemiesInConeCount(const Player& p, size_t idx, float range,
                           float halfAngleDeg) const;
    void dealMatchDamage(size_t attackerIdx, size_t victimIdx, float dmg);
    // Wave 42 fix: the rock's frontal shield, shared by kit damage
    // (dealMatchDamage) and melee auto-attacks. Halves `final` when the
    // attacker stands in the holder's frontal 120° arc, and drains
    // durability in proportion to the blocked damage — about 6 blocked
    // level-appropriate melee hits break a fresh rock.
    void applyRockShield(size_t victimIdx, const Vec3& attackerPos,
                         float& final);
    // slot: 0=Q 1=F 2=R. Returns true when the cast went off.
    bool castMatchSpell(size_t idx, int slot);
    bool castMatchRmb(size_t idx, RmbAim aim = RmbAim::Normal);
    // Wave 42: Cthulhu's rock — rip, swing, launch.
    bool ripRock(size_t idx);
    bool swingRock(size_t idx);
    bool throwRock(size_t idx);
    void breakRock(size_t idx, const char* why);
    void updateBotAbilities(Player& p, size_t idx, double dt);
    CharacterRegistry kits_;
    size_t kitAssignCounter_ = 0;
    // Wave 42: shuffled kit deck — every match deals a fresh random
    // composition (replayability). Rebuilt in start().
    std::vector<std::string> shuffledKits_;
};

} // namespace cultulhu
