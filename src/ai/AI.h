#pragma once

#include "entities/Entity.h"
#include "entities/Units.h"

#include <vector>

namespace cultulhu {

// Basic steering behaviors. The engine binding (Unreal) can replace these
// with navmesh movement; the decision logic stays here.
namespace ai {

// Move an entity toward a target point.
void moveToward(Entity& e, const Vec3& target, double dt, float speed);

// Cultist follows its leader / receives orders to hold a position.
void cultistFollow(Cultist& c, const Vec3& leaderPos, double dt,
                   float speed = 4.0f);

// Civilian flees directly away from a threat.
void civilianFlee(Civilian& c, const Vec3& threatPos, double dt,
                  float speed = 5.0f);

// Feral monstrosity rampage targeting: nearest entity of Cthulhu's faction
// (typically cultists), excluding itself. Returns nullptr if none.
const Entity* selectRampageTarget(
    const Monstrosity& m, const std::vector<const Entity*>& entities);

// One rampage step: pick the target and move toward it.
void rampageStep(Monstrosity& m, const std::vector<const Entity*>& entities,
                 double dt, float speed = 6.0f);

} // namespace ai
} // namespace cultulhu
