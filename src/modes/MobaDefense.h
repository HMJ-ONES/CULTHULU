#pragma once

#include "core/RNG.h"
#include "core/Vec3.h"
#include "modes/GameMode.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cultulhu {

// 5v5 MOBA defense (wave-2 expanded): three-lane-ready map, defensive towers
// per team per lane, and minion wave composition (melee / ranged / siege).
// Each team fields a Great Old One and a base; minion waves march down lanes
// on the enemy base. Lose if your Great Old One falls.
class MobaDefense : public GameMode {
public:
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
        float hp = 600.0f;
        float maxHp = 600.0f;
        float range = 12.0f;
        float dps = 25.0f;
        bool alive() const { return hp > 0.0f; }
    };

    static constexpr double WAVE_INTERVAL = 30.0; // seconds between waves
    static constexpr float BASE_DPS_TO_STRUCTURE = 10.0f;
    static constexpr int TOWERS_PER_TEAM_PER_LANE = 2;

    MobaDefense(EventBus& bus, GameClock& clock, RNG& rng);

    void addLane(const std::vector<Vec3>& waypoints);
    // Register a team's base: position, structure HP, and its Great Old One.
    void setBase(int team, Vec3 pos, float hp);

    void spawnWave(int team, int lane); // also called automatically on timer
    void update(double dt) override;
    bool isOver() const override;
    int winner() const override;

    size_t minionCount() const { return minions_.size(); }
    size_t laneCount() const { return lanes_.size(); }
    size_t towerCount() const { return towers_.size(); }
    float baseHp(int team) const;
    float towerHp(int team, int lane, int idx) const;
    const std::vector<Minion>& minions() const { return minions_; }

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
    uint64_t nextMinionId_ = 1;
    uint64_t waveNumber_ = 0;
    bool towersBuilt_ = false;

    void ensureTowers();
    Vec3 pointAtFraction(int lane, float f) const;
    Vec3 pathTarget(const Minion& m) const; // next waypoint (team-aware)
    bool pathComplete(const Minion& m) const;
    void damageBase(int team, float dmg);
    Minion makeMinion(int team, int lane, const std::string& kind, Vec3 pos);
};

} // namespace cultulhu
