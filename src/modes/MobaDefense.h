#pragma once

#include "core/RNG.h"
#include "core/Vec3.h"
#include "modes/GameMode.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace cultulhu {

// 5v5 MOBA defense (wave-2 expanded): three-lane-ready map, defensive towers
// per team per lane, and minion wave composition (melee / ranged / siege).
// Each team fields a Great Old One and a base; minion waves march down lanes
// on the enemy base. Lose if your Great Old One falls.
//
// Wave 21 additions:
//   - setupDefaultMap(): bases at (-150,0,0)/(150,0,0); the Great Old One
//     stat block is 5000 HP; 3 lanes (z=-40,0,40) with the mid waypoint
//     bowed outward ((0,0,-55) / (0,0,55) / (0,0,55)).
//   - Backdoor protection: damageBase() does nothing while any tower of the
//     defending team is still alive — the GOO is only killable once its
//     team's towers are down.
//   - Players fight: Match feeds player positions via setPlayerTargets();
//     minions and towers target the nearest enemy (players included), and
//     player melee damage to structures/minions goes through
//     playerHitStructures()/playerHitMinions(). Enemy damage to players is
//     reported back via the player-damage hook so Match owns deaths/KDA.
//   - Minion cap: waves are skipped while a team fields more than 48 live
//     minions (keeps the host sim light).
class MobaDefense : public GameMode {
public:
    // A match player, as seen by minions and towers. Match populates this
    // each tick via setPlayerTargets().
    struct PlayerTarget {
        uint64_t id = 0;
        int team = 0;
        Vec3 pos;
        float hp = 100.0f;
        bool alive = true;
    };
    struct Minion {
        uint64_t id = 0;
        int team = 0;
        int lane = 0;
        size_t step = 0;      // waypoints consumed along the path
        Vec3 pos;
        std::string kind;     // "melee" | "ranged" | "siege"
        float hp = 100.0f;
        float maxHp = 100.0f;
        float dps = 8.0f;
        float range = 4.0f;
        float speed = 3.0f;
    };

    struct Tower {
        int team = -1;
        int lane = 0;
        Vec3 pos;
        float hp = 250.0f;
        float maxHp = 250.0f;
        float range = 12.0f;
        float dps = 18.0f;
        bool alive() const { return hp > 0.0f; }
    };

    static constexpr double WAVE_INTERVAL = 30.0; // seconds between waves
    static constexpr float BASE_DPS_TO_STRUCTURE = 15.0f;
    static constexpr int TOWERS_PER_TEAM_PER_LANE = 2;
    static constexpr float GOO_MAX_HP = 1500.0f; // Great Old One stat block
    static constexpr int MINION_CAP_PER_TEAM = 48; // light budget: skip waves above this
    static constexpr double TIME_LIMIT = 1200.0; // 20 min; then GOO-HP fraction decides

    MobaDefense(EventBus& bus, GameClock& clock, RNG& rng);

    void addLane(const std::vector<Vec3>& waypoints);
    // Register a team's base: position, structure HP, and its Great Old One.
    void setBase(int team, Vec3 pos, float hp);
    // Wave 21: standard 5v5 map (see class comment).
    void setupDefaultMap();

    void spawnWave(int team, int lane); // also called automatically on timer
    void update(double dt) override;
    bool isOver() const override;
    int winner() const override;

    size_t minionCount() const { return minions_.size(); }
    size_t laneCount() const { return lanes_.size(); }
    size_t towerCount() const { return towers_.size(); }
    float baseHp(int team) const;
    Vec3 basePos(int team) const { return bases_[team].pos; }
    float towerHp(int team, int lane, int idx) const;
    Vec3 towerPos(int team, int lane, int idx) const;
    const std::vector<Minion>& minions() const { return minions_; }

    // --- Wave 21 player hooks (called by Match) ---
    // Refresh the player list minions/towers can target. The vector is
    // copied (10 players max; no per-frame allocation on the caller's side).
    void setPlayerTargets(const std::vector<PlayerTarget>& targets);
    // (targetPlayerId, damage, attackerEntityId). The match applies the
    // damage and resolves deaths (PlayerKilled / KDA stay in Match).
    void setPlayerDamageHook(
        std::function<void(uint64_t, float, uint64_t)> hook);
    // Player melee vs minions / towers / enemy base (GOO). attackerId is the
    // player id — it becomes GreatOldOneSlain.sourceId (Godslayer).
    void playerHitMinions(int attackerTeam, Vec3 pos, float range, float dmg);
    void playerHitStructures(int attackerTeam, Vec3 pos, float range,
                             float dmg, uint64_t attackerId);
    // Total HP of live enemy towers of team `defender` within range of pos
    // (lets Match decide whether a swing actually hit something).
    float enemyTowerHpNear(int attackerTeam, Vec3 pos, float range) const;
    // Team-aware point along a lane's path (fraction 0..1 from that team's
    // own base): bot marching target.
    Vec3 lanePoint(int team, int lane, float fraction) const;
    // Number of live minions fielded by one team (for the wave cap).
    int liveMinionCount(int team) const;
    bool teamTowerAlive(int team) const;
    // Backdoor protection (wave 22): the GOO can be damaged once ANY one
    // lane has both its towers down — no need to raze all six.
    bool laneOpenFor(int team) const;
    // Both towers in (team, lane) down. Vacuously true before towers exist.
    bool laneTowersDown(int team, int lane) const;
    int aliveTowerCount(int team) const;
    // Nearest live enemy tower within range of pos (for bot sieging).
    bool nearestEnemyTowerPos(int team, Vec3 pos, float range,
                              Vec3& out) const;
    // Team-oriented lane fraction (0 = own base, 1 = enemy base) of the
    // point on the lane nearest to pos (for bot marching/escorting).
    float laneFraction(int team, int lane, Vec3 pos) const;
    double elapsed() const { return elapsed_; }

protected:
    struct Base {
        int team = -1;
        Vec3 pos;
        float hp = 2000.0f;
        float maxHp = 2000.0f;
        bool gooAlive = true;
        bool set = false;
    };

    std::vector<std::vector<Vec3>> lanes_;
    std::vector<Minion> minions_;
    std::vector<Tower> towers_;
    Base bases_[2];
    RNG& rng_;
    double waveTimer_ = 0.0;
    double elapsed_ = 0.0;
    uint64_t nextMinionId_ = 1;
    uint64_t waveNumber_ = 0;
    bool towersBuilt_ = false;
    // Wave 21: players in the match, refreshed each tick by Match.
    std::vector<PlayerTarget> playerTargets_;
    std::function<void(uint64_t, float, uint64_t)> playerDamageHook_;
    // Tower entity ids for the damage hook (stable per tower index).
    static constexpr uint64_t kTowerIdBase = 5000;

    void ensureTowers();
    Vec3 pointAtFraction(int lane, float f) const;
    Vec3 pathTarget(const Minion& m) const; // next waypoint (team-aware)
    bool pathComplete(const Minion& m) const;
    void damageBase(int team, float dmg, uint64_t attackerId = 0);
    Minion makeMinion(int team, int lane, const std::string& kind, Vec3 pos,
                      float empower = 1.0f);
};

} // namespace cultulhu
