#include "dreams/DreamSystem.h"

#include "beliefs/BeliefSystem.h"
#include "core/EventBus.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "entities/Units.h"

namespace cultulhu {

namespace {
// Nightmare chance multiplier while the Chaos belief is active (lunatic
// synergy, per the Dreams design).
constexpr float CHAOS_NIGHTMARE_MULT = 3.0f;
} // namespace

DreamSystem::DreamSystem(EventBus& bus, RNG& rng, BeliefSystem& beliefs,
                         CultManager& cult, float powerPerSec,
                         float convertChance, float nightmareChance)
    : bus_(bus), rng_(rng), beliefs_(beliefs), cult_(cult),
      powerPerSec_(powerPerSec), convertChance_(convertChance),
      nightmareChance_(nightmareChance) {}

void DreamSystem::startRest(uint64_t cultistId) {
    if (resting_.insert(cultistId).second) {
        GameEvent e(EventType::RestStarted);
        e.sourceId = cultistId;
        bus_.publish(e);
    }
}

void DreamSystem::endRest(uint64_t cultistId) {
    if (resting_.erase(cultistId) > 0) {
        GameEvent e(EventType::RestEnded);
        e.sourceId = cultistId;
        bus_.publish(e);
    }
}

bool DreamSystem::isResting(uint64_t cultistId) const {
    return resting_.count(cultistId) > 0;
}

float DreamSystem::update(double dt) {
    if (resting_.empty()) return 0.0f;
    if (!beliefs_.isActive(Belief::Dreams)) return 0.0f;

    float delta = powerPerSec_ * (night_ ? 1.5f : 1.0f) *
                  static_cast<float>(resting_.size()) *
                  static_cast<float>(dt);

    const bool chaos = beliefs_.isActive(Belief::Chaos);
    for (uint64_t id : resting_) {
        // Dream-whispers convert distant civilians.
        float cp = convertChance_ * static_cast<float>(dt);
        if (cp > 1.0f) cp = 1.0f;
        if (rng_.chance(cp)) {
            const uint64_t civ = picker_ ? picker_() : 0;
            GameEvent w(EventType::DreamWhisper);
            w.sourceId = id;
            w.targetId = civ;
            w.amount = 1.0f;
            bus_.publish(w);
            // Route through the normal conversion event so the Conversion
            // belief's power rule fires when it is active.
            GameEvent c(EventType::ConversionPerformed);
            c.sourceId = id;
            c.targetId = civ;
            c.amount = 1.0f;
            c.tag = "dream_whisper";
            bus_.publish(c);
        }

        // Nightmares: the cultist wakes up Lunatic.
        float np = nightmareChance_ * (chaos ? CHAOS_NIGHTMARE_MULT : 1.0f) *
                   static_cast<float>(dt);
        if (np > 1.0f) np = 1.0f;
        if (rng_.chance(np)) {
            for (size_t i = 0; i < cult_.size(); ++i) {
                Cultist& c = cult_.at(i);
                if (c.id() == id && c.alive() &&
                    c.state() != CultistState::Lunatic) {
                    c.setState(CultistState::Lunatic);
                    GameEvent e(EventType::Nightmare);
                    e.sourceId = id;
                    bus_.publish(e);
                    break;
                }
            }
        }
    }
    return delta;
}

} // namespace cultulhu
