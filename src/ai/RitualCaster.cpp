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
    if (timer_ < interval_) return;
    while (timer_ >= interval_) timer_ -= interval_;

    float chance = exertion_.stats().conversionChance;
    // Oneiromancy (Magic x Dreams): dream-touched rituals convert better.
    if (synergyActive(Belief::Magic, Belief::Dreams, exertion_.levels()))
        chance = std::min(0.95f, chance + 0.05f);

    for (Sorcerer* s : sorcerers_) {
        if (!s || !s->alive() || s->mana() < MANA_COST) continue;
        // Nearby civilians only — rituals need an audience.
        std::vector<Civilian*> near;
        for (Civilian* c : civilians_) {
            if (!c || !c->alive()) continue;
            const float dx = c->position().x - s->position().x;
            const float dz = c->position().z - s->position().z;
            if (std::sqrt(dx * dx + dz * dz) <= RANGE) near.push_back(c);
        }
        if (near.empty()) continue;

        Civilian* target = near[rng_.intRange(
            0, static_cast<int>(near.size()) - 1)];
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
            bus_.publish(conv);
        }
    }
}

} // namespace cultulhu
