#pragma once

#include "core/RNG.h"
#include "core/Vec3.h"
#include "entities/Entity.h"

#include <vector>

namespace cultulhu {

// A free-roam "bot": an AI-controlled rival (witch-hunter / rival cultist)
// that wanders the wilds, hunts the player's avatar and wild creatures,
// and flees when badly hurt. This is the free-roam counterpart to the
// match-mode bots:
//   - bot vs bot: rival bots fight wild creatures (both AI)
//   - player vs bot: the avatar (or its cultists) fight rival bots
// Pure logic; the driver/UE5 binding owns rendering and damage routing
// for the avatar (passed in via Context).
class RivalBot : public Entity {
public:
    enum class State { Wander, Engage, Flee };

    static constexpr float MAX_HP = 120.0f;
    static constexpr float SPEED = 4.5f;
    static constexpr float MELEE_RANGE = 3.5f;
    static constexpr float MELEE_DAMAGE = 12.0f; // per 1s swing
    static constexpr float SIGHT_RANGE = 20.0f;
    static constexpr float FLEE_HP_FRACTION = 0.35f;

    // Per-tick world context (not owned).
    struct Context {
        Entity* avatar = nullptr; // player's avatar; null = no player
        std::vector<Entity*> creatures; // hostile wild creatures
    };

    explicit RivalBot(Vec3 pos, RNG& rng)
        : Entity(EntityType::Rival, FACTION_RIVAL, pos, MAX_HP),
          rng_(rng) {}

    State state() const { return state_; }

    void update(double dt, const Context& ctx);

private:
    RNG& rng_;
    State state_ = State::Wander;
    Vec3 wanderTarget_{0, 0, 0};
    bool hasWanderTarget_ = false;
    double idleTimer_ = 0.0;
    double attackCd_ = 0.0;

    Entity* nearestEnemy(const Context& ctx) const;
    void updateWander(double dt);
    void updateEngage(double dt, Entity& enemy);
    void updateFlee(double dt, const Entity& threat);
};

} // namespace cultulhu
