#pragma once

// Wave 4: belief exertion (fervor). Every belief carries a 0..100 exertion
// meter fed by gameplay actions through ACTION_EXERTION_TABLE. Active
// beliefs accumulate at full rate; inactive beliefs still track at half
// rate, so switching beliefs mid-game has momentum instead of starting
// cold. Exertion decays back toward a baseline when neglected.
//
// The ExertionSystem also owns the unified power pipeline: every game event
// flows through BeliefSystem::onEvent here, synergy multipliers from the
// interaction matrix are applied, and the result lands in PowerSystem.
// (Previously the driver did this inline; moving it here keeps power rules
// and exertion bonuses in one place.)
//
// Derived stats (combat power, loyalty drift, conversion chance) are
// recomputed every tick from exertion and exposed for other systems.

#include "beliefs/Belief.h"
#include "beliefs/BeliefSystem.h"
#include "beliefs/InteractionMatrix.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "exertion/DerivedStats.h"
#include "power/PowerSystem.h"

namespace cultulhu {

class ExertionSystem {
public:
    static constexpr float MIN_EXERTION = 0.0f;
    static constexpr float MAX_EXERTION = 100.0f;
    static constexpr float BASELINE = 10.0f;      // rest level, decays here
    static constexpr float DECAY_PER_SEC = 0.5f;  // drift toward baseline
    static constexpr float SYNERGY_THRESHOLD = 50.0f; // synergy/conflict gate
    static constexpr float TENSION_THRESHOLD = 70.0f; // tension event gate
    static constexpr float TENSION_COOLDOWN = 60.0f;  // seconds between
                                                      // tension events

    ExertionSystem(EventBus& bus, BeliefSystem& beliefs, PowerSystem& power,
                  CultManager& cult, RNG& rng);

    float exertion(Belief b) const;
    const float* levels() const { return exertion_; } // for synergy queries
    const DerivedStats& stats() const { return stats_; }

    // Add exertion (may be negative). Inactive beliefs accumulate at half
    // rate; conflicting beliefs above threshold halve positive gains.
    void addExertion(Belief b, float amount);

    // Unified power pipeline entry point: feed exertion from the event,
    // run the existing belief power rules, apply synergy multipliers.
    void onGameEvent(const GameEvent& e);

    // Decay, derived stats, loyalty drift, tension events, synergy upkeep.
    void update(double dt);

private:
    void feedFromEvent(const GameEvent& e);
    void feedFromDirective(const GameEvent& e); // parses DirectiveResolved
    float powerMultiplierFor(const GameEvent& e) const;
    void fireTensions(double dt);

    EventBus& bus_;
    BeliefSystem& beliefs_;
    PowerSystem& power_;
    CultManager& cult_;
    RNG& rng_;

    float exertion_[static_cast<int>(Belief::Count)];
    DerivedStats stats_;

    // Per-conflict-pair tension cooldowns (indexed like conflictPairs()).
    double tensionCooldown_[16] = {};
};

} // namespace cultulhu
