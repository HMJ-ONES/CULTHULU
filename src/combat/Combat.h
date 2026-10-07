#pragma once

#include "beliefs/BeliefSystem.h"
#include "core/Events.h"
#include "entities/Entity.h"

namespace cultulhu {

struct AttackInfo {
    float baseDamage = 0.0f;
    bool melee = false;
    EntityType attackerType = EntityType::Cultist;
};

namespace combat {

// Final damage after belief modifiers (Onslaught melee bonus, Magic sorcerer
// bonus, Reconstruction spell penalty).
float calcDamage(const AttackInfo& atk, const BeliefSystem& beliefs);

// Crowd-control duration after belief modifiers (Magic shortens foe CC).
float ccDuration(float baseSeconds, const BeliefSystem& beliefs);

// Applies damage; publishes `killedEvent` on the bus if the target dies.
bool dealDamage(Entity& target, float dmg, EventBus& bus,
                EventType killedEvent, bool oneHit = false);

} // namespace combat
} // namespace cultulhu
