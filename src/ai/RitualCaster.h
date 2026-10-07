#pragma once

// Wave 4: ambient magic conversion rituals. Sorcerers in the world
// periodically attempt to convert nearby civilians through ritual magic —
// the "random magic conversion rituals" complement to player-directed
// conversion campaigns. Success uses the exertion-derived conversion
// chance (Conversion/Trickery/Magic exertion + Infiltration synergy,
// minus Chaos Sabotage), so the cult's creed literally shapes how well
// its sorcerers convert.

#include "core/Events.h"
#include "entities/Units.h"

#include <cstdint>
#include <vector>

namespace cultulhu {

class EventBus;
class RNG;
class BeliefSystem;
class ExertionSystem;

class RitualCaster {
public:
    RitualCaster(EventBus& bus, RNG& rng, BeliefSystem& beliefs,
                 ExertionSystem& exertion, double intervalSeconds = 45.0);

    // World state (raw pointers; the driver refreshes these each tick).
    void setSorcerers(const std::vector<Sorcerer*>& s) { sorcerers_ = s; }
    void setCivilians(const std::vector<Civilian*>& c) { civilians_ = c; }

    void update(double dt);

    uint64_t ritualsAttempted() const { return attempted_; }
    uint64_t ritualsSucceeded() const { return succeeded_; }

private:
    EventBus& bus_;
    RNG& rng_;
    BeliefSystem& beliefs_;
    ExertionSystem& exertion_;

    std::vector<Sorcerer*> sorcerers_;
    std::vector<Civilian*> civilians_;

    double interval_;
    double timer_ = 0.0;
    uint64_t attempted_ = 0;
    uint64_t succeeded_ = 0;

    static constexpr float RANGE = 200.0f;
    static constexpr float MANA_COST = 25.0f;
};

} // namespace cultulhu
