#include "combat/Attacks.h"
#include "beliefs/BeliefSystem.h"
#include "combat/Combat.h"
#include "core/EventBus.h"

namespace cultulhu {

const char* damageTypeName(DamageType type) {
    switch (type) {
        case DamageType::Physical: return "Physical";
        case DamageType::Fire:     return "Fire";
        case DamageType::Shadow:   return "Shadow";
        case DamageType::Holy:     return "Holy";
        case DamageType::Count:    break;
    }
    return "Unknown";
}

float resolveAttack(const Attack& atk, Entity& target,
                    const BeliefSystem& beliefs, EventBus& bus,
                    ActiveEffects& fx, EventType killedEvent) {
    (void)fx; // reserved for future on-hit CC procs
    AttackInfo info;
    info.baseDamage = atk.baseDamage;
    info.melee = atk.melee;
    info.attackerType = atk.attackerType;
    float dmg = combat::calcDamage(info, beliefs);
    combat::dealDamage(target, dmg, bus, killedEvent);
    return dmg;
}

float strikeMelee(uint64_t attackerId, EntityType attackerType, Entity& target,
                  float baseDamage, const BeliefSystem& beliefs,
                  EventBus& bus, ActiveEffects& fx, EventType killedEvent) {
    Attack atk;
    atk.attackerId = attackerId;
    atk.attackerType = attackerType;
    atk.damageType = DamageType::Physical;
    atk.baseDamage = baseDamage;
    atk.melee = true;
    atk.range = 2.0f;
    return resolveAttack(atk, target, beliefs, bus, fx, killedEvent);
}

float castSpell(uint64_t casterId, EntityType casterType, Entity& target,
                float baseDamage, DamageType damageType,
                CCType cc, float ccSeconds, const BeliefSystem& beliefs,
                EventBus& bus, ActiveEffects& fx, EventType killedEvent) {
    Attack atk;
    atk.attackerId = casterId;
    atk.attackerType = casterType;
    atk.damageType = damageType;
    atk.baseDamage = baseDamage;
    atk.melee = false;
    atk.range = 30.0f;
    float dmg = resolveAttack(atk, target, beliefs, bus, fx, killedEvent);
    // CC lands on survivors only; a corpse needs no crowd control.
    if (target.alive() && ccSeconds > 0.0f)
        fx.apply(target.id(), cc, ccSeconds, beliefs, bus);
    return dmg;
}

} // namespace cultulhu
