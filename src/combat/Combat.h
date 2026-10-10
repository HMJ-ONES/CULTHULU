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
// When the killer is known, pass its entity id and type: avatar-on-avatar
// kills additionally publish PlayerKilled (PvP), and monstrosity kills
// publish MonstrositySlain (tag = species).
bool dealDamage(Entity& target, float dmg, EventBus& bus,
                EventType killedEvent, bool oneHit = false,
                uint64_t attackerId = 0,
                EntityType attackerType = EntityType::Creature);

} // namespace combat
} // namespace cultulhu
