#include "combat/Combat.h"
#include "core/EventBus.h"

namespace cultulhu {
namespace combat {

float calcDamage(const AttackInfo& atk, const BeliefSystem& beliefs) {
    float dmg = atk.baseDamage;
    if (atk.melee) dmg *= beliefs.meleeBonus();                    // Onslaught
    if (atk.attackerType == EntityType::Sorcerer)
        dmg *= beliefs.sorcererDamageMult();                      // Magic
    dmg *= beliefs.spellEffectiveness();                          // Reconstruction
    return dmg;
}

float ccDuration(float baseSeconds, const BeliefSystem& beliefs) {
    return baseSeconds * beliefs.foeCcDurationMult();             // Magic
}

bool dealDamage(Entity& target, float dmg, EventBus& bus,
                EventType killedEvent, bool oneHit) {
    bool killed = target.takeDamage(dmg, oneHit);
    if (killed) {
        GameEvent e(killedEvent);
        e.targetId = target.id();
        e.faction = target.faction();
        bus.publish(e);
    }
    return killed;
}

} // namespace combat
} // namespace cultulhu
