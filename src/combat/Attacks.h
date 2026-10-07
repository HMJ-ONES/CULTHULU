#pragma once

#include "combat/CrowdControl.h"
#include "core/Events.h"
#include "entities/Entity.h"

#include <cstdint>

namespace cultulhu {

class BeliefSystem;
class EventBus;

// Elemental/physical damage taxonomy.
enum class DamageType {
    Physical,
    Fire,
    Shadow,
    Holy,
    Count
};

// Human-readable name for a damage type.
const char* damageTypeName(DamageType type);

// A single combat strike or spell to resolve.
struct Attack {
    uint64_t attackerId = 0;
    EntityType attackerType = EntityType::Cultist;
    DamageType damageType = DamageType::Physical;
    float baseDamage = 10.0f;
    bool melee = true;
    float range = 2.0f;
};

// Tuning notes (creative liberty):
//  - Damage types are currently a taxonomy hook for a future resistance /
//    vulnerability system: they carry flavor through attacks and spells but
//    do NOT modify damage today. Belief scaling (Onslaught 1.25x melee,
//    Magic 1.3x sorcerer, Reconstruction 0.75x spells) still comes from
//    combat::calcDamage via attackerType/melee, not damage type, so a
//    Shadow spell from a Sorcerer benefits from Magic exactly once.
//  - Default baseDamage (10) sits between a Civilian's 50 HP (5 hits) and
//    a Cultist's 100 HP (10 hits); melee range 2.0 matches adjacent tiles.
//  - castSpell applies its CC only if the target survives the hit: applying
//    crowd control to a corpse is pointless noise on the event bus.
//  - CC durations in castSpell go through combat::ccDuration, so an active
//    Magic belief shortens CC landed on foes (0.6x) even mid-combo.
float resolveAttack(const Attack& atk, Entity& target,
                    const BeliefSystem& beliefs, EventBus& bus,
                    ActiveEffects& fx, EventType killedEvent);

// Convenience wrapper: physical melee strike at the default range.
float strikeMelee(uint64_t attackerId, EntityType attackerType, Entity& target,
                  float baseDamage, const BeliefSystem& beliefs,
                  EventBus& bus, ActiveEffects& fx, EventType killedEvent);

// Resolve ranged spell damage, then apply crowd control through the active
// effects system (demonstrating Magic-belief CC reduction).
float castSpell(uint64_t casterId, EntityType casterType, Entity& target,
                float baseDamage, DamageType damageType,
                CCType cc, float ccSeconds, const BeliefSystem& beliefs,
                EventBus& bus, ActiveEffects& fx, EventType killedEvent);

} // namespace cultulhu
