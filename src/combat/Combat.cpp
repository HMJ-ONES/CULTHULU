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
                EventType killedEvent, bool oneHit, uint64_t attackerId,
                EntityType attackerType) {
    bool killed = target.takeDamage(dmg, oneHit);
    // Wave 18: a monstrosity/feral beast tearing into a human MAULS them.
    // The AnimationDirector consumes this and poses the ATTACKER (Maul is
    // a 0.9s one-shot that auto-returns to Idle). Humans: civilians,
    // cultists, adventurers — the prey of beasts.
    if (attackerType == EntityType::Monstrosity &&
        (target.type() == EntityType::Civilian ||
         target.type() == EntityType::Cultist ||
         target.type() == EntityType::Adventurer)) {
        GameEvent m(EventType::MaulStruck);
        m.sourceId = attackerId;
        m.targetId = target.id();
        bus.publish(m);
    }
    if (killed) {
        GameEvent e(killedEvent);
        e.targetId = target.id();
        e.faction = target.faction();
        bus.publish(e);
        // Wave 16: species-tagged monstrosity slay (e.g. "dhole").
        if (target.type() == EntityType::Monstrosity) {
            GameEvent m(EventType::MonstrositySlain);
            m.targetId = target.id();
            m.faction = target.faction();
            m.tag = target.species();
            bus.publish(m);
        }
        // Wave 16: avatar-on-avatar kill = PvP kill.
        if (attackerId != 0 && attackerId != target.id() &&
            attackerType == EntityType::EldritchAvatar &&
            target.type() == EntityType::EldritchAvatar) {
            GameEvent p(EventType::PlayerKilled);
            p.sourceId = attackerId;
            p.targetId = target.id();
            bus.publish(p);
        }
    }
    return killed;
}

} // namespace combat
} // namespace cultulhu
