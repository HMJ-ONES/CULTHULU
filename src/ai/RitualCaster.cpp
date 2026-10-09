#include "ai/RitualCaster.h"

#include "beliefs/BeliefSystem.h"
#include "beliefs/InteractionMatrix.h"
#include "core/EventBus.h"
#include "core/RNG.h"
#include "core/Vec3.h"
#include "exertion/ExertionSystem.h"

#include <algorithm>
#include <cmath>

namespace cultulhu {

RitualCaster::RitualCaster(EventBus& bus, RNG& rng, BeliefSystem& beliefs,
                           ExertionSystem& exertion, double intervalSeconds)
    : bus_(bus), rng_(rng), beliefs_(beliefs), exertion_(exertion),
      interval_(intervalSeconds) {}

void RitualCaster::update(double dt) {
    timer_ += dt;
    // Wave 30: directed rituals expire after two minutes.
    if (hasDirected_) {
        directedAge_ += dt;
        if (directedAge_ > 120.0) hasDirected_ = false;
    }
    if (timer_ < interval_) return;
    while (timer_ >= interval_) timer_ -= interval_;

    float chance = exertion_.stats().conversionChance;
    // Oneiromancy (Magic x Dreams): dream-touched rituals convert better.
    if (synergyActive(Belief::Magic, Belief::Dreams, exertion_.levels()))
        chance = std::min(0.95f, chance + 0.05f);

    for (Sorcerer* s : sorcerers_) {
        if (!s || !s->alive() || s->mana() < MANA_COST) continue;
        // Nearby civilians only — rituals need an audience. Wave 16:
        // enemy cultists in range may also be turned (Turncoat).
        std::vector<Entity*> near;
        for (Civilian* c : civilians_) {
            if (!c || !c->alive()) continue;
            const float dx = c->position().x - s->position().x;
            const float dz = c->position().z - s->position().z;
            if (std::sqrt(dx * dx + dz * dz) <= RANGE) near.push_back(c);
        }
        for (Cultist* c : cultists_) {
            if (!c || !c->alive()) continue;
            if (c->faction() == FACTION_NEUTRAL) continue;
            if (c->faction() == s->faction()) continue; // already ours
            const float dx = c->position().x - s->position().x;
            const float dz = c->position().z - s->position().z;
            if (std::sqrt(dx * dx + dz * dz) <= RANGE) near.push_back(c);
        }
        if (near.empty()) continue;

        Entity* target;
        if (hasDirected_) {
            // Wave 30: aim at the soul nearest the directed point.
            target = near[0];
            float best = 1e9f;
            for (Entity* e : near) {
                const float dx = e->position().x - directed_.x;
                const float dz = e->position().z - directed_.z;
                const float d = std::sqrt(dx * dx + dz * dz);
                if (d < best) { best = d; target = e; }
            }
        } else {
            target = near[rng_.intRange(
                0, static_cast<int>(near.size()) - 1)];
        }
        s->setMana(s->mana() - MANA_COST);
        ++attempted_;

        // The ritual itself is a spell cast (feeds Magic exertion).
        GameEvent cast(EventType::SpellCast);
        cast.sourceId = s->id();
        cast.targetId = target->id();
        cast.tag = "conversion_ritual";
        cast.amount = 1.0f;
        bus_.publish(cast);

        if (rng_.chance(chance)) {
            ++succeeded_;
            GameEvent conv(EventType::ConversionPerformed);
            conv.sourceId = s->id();
            conv.targetId = target->id();
            conv.amount = 1.0f;
            conv.tag = "ritual";
            conv.faction = target->faction(); // wave 16: who was turned
            bus_.publish(conv);
        }
    }
}

} // namespace cultulhu
