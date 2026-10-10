// Wave 9a: headless balance-simulation harness. SimWorld wires the core
// gameplay systems together (BeliefSystem + ExertionSystem + PowerSystem +
// CultManager + EventBus) with a seeded RNG. No rendering, no driver, no
// network — just the rules, advanced one game-second per tick.
//
// A scenario's belief loadout is activated immediately via restoreActive()
// (no adoption timers): scenarios describe a steady-state creed, not the
// belief-switching transition.

#pragma once

#include "beliefs/BeliefSystem.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "exertion/ExertionSystem.h"
#include "power/PowerSystem.h"

#include <cstdint>
#include <vector>

namespace cultulhu {
namespace sim {

class SimWorld {
public:
    // Number of cultists the harness starts with (a small congregation so
    // death-by-cause events have victims to claim).
    static constexpr int START_CULTISTS = 12;

    SimWorld(uint64_t seed, const std::vector<Belief>& loadout);

    // Advance one game-second through every system.
    void tick(double dt = 1.0);

    EventBus& bus() { return bus_; }
    GameClock& clock() { return clock_; }
    RNG& rng() { return rng_; }
    BeliefSystem& beliefs() { return beliefs_; }
    PowerSystem& power() { return power_; }
    CultManager& cult() { return cult_; }
    ExertionSystem& exertion() { return exertion_; }

    // Kill one live cultist outright. Returns false when none are left
    // (callers should only publish a death-cause event on success so the
    // "deaths by cause" metric stays 1:1 with actual deaths).
    bool killOneCultist();

private:
    EventBus bus_;
    GameClock clock_;
    RNG rng_;
    BeliefSystem beliefs_;
    PowerSystem power_;
    CultManager cult_;
    ExertionSystem exertion_;
};

} // namespace sim
} // namespace cultulhu
