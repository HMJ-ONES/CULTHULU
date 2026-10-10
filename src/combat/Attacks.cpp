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
    // Wave 4: exertion-derived faction power scales the final damage.
    float dmg = combat::calcDamage(info, beliefs) * atk.factionPowerMult;
    // Wave 18: cultist-on-cultist melee is a war engagement (disciplined
    // fighting, not a brawl, not a beast maul). Brawls never resolve
    // combat and mauls use Monstrosity attackers, so this stays clean.
    if (atk.melee && atk.attackerType == EntityType::Cultist &&
        target.type() == EntityType::Cultist) {
        GameEvent war(EventType::WarEngagement);
        war.sourceId = atk.attackerId;
        war.targetId = target.id();
        war.faction = target.faction();
        bus.publish(war);
    }
    combat::dealDamage(target, dmg, bus, killedEvent, false, atk.attackerId,
                       atk.attackerType);
    return dmg;
}

float strikeMelee(uint64_t attackerId, EntityType attackerType, Entity& target,
                  float baseDamage, const BeliefSystem& beliefs,
                  EventBus& bus, ActiveEffects& fx, EventType killedEvent,
                  float factionPowerMult) {
    Attack atk;
    atk.attackerId = attackerId;
    atk.attackerType = attackerType;
    atk.damageType = DamageType::Physical;
    atk.baseDamage = baseDamage;
    atk.melee = true;
    atk.range = 2.0f;
    atk.factionPowerMult = factionPowerMult;
    return resolveAttack(atk, target, beliefs, bus, fx, killedEvent);
}

float castSpell(uint64_t casterId, EntityType casterType, Entity& target,
                float baseDamage, DamageType damageType,
                CCType cc, float ccSeconds, const BeliefSystem& beliefs,
                EventBus& bus, ActiveEffects& fx, EventType killedEvent,
                float factionPowerMult) {
    Attack atk;
    atk.attackerId = casterId;
    atk.attackerType = casterType;
    atk.damageType = damageType;
    atk.baseDamage = baseDamage;
    atk.melee = false;
    atk.range = 30.0f;
    atk.factionPowerMult = factionPowerMult;
    float dmg = resolveAttack(atk, target, beliefs, bus, fx, killedEvent);
    // CC lands on survivors only; a corpse needs no crowd control.
    if (target.alive() && ccSeconds > 0.0f)
        fx.apply(target.id(), cc, ccSeconds, beliefs, bus);
    return dmg;
}

} // namespace cultulhu
