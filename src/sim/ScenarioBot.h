// Wave 9a: ScenarioBot — the scripted actor inside the balance sim. Each
// scenario is a belief loadout; the bot emits a fixed schedule of
// belief-relevant actions through the event bus every tick, so every system
// (power, exertion, insurrection, derived stats) is exercised the same way
// each run. Multi-belief scenarios run the union of the single-belief
// schedules. All jitter draws from the world's seeded RNG, keeping runs
// deterministic per (scenario, seed).

#pragma once

#include "beliefs/Belief.h"
#include "core/Events.h"

#include <string>
#include <vector>

namespace cultulhu {
namespace sim {

class SimWorld;

class ScenarioBot {
public:
    ScenarioBot(SimWorld& world, const std::vector<Belief>& loadout);

    // Emit this tick's scheduled actions. t is the 0-based tick index.
    void tick(int t);

private:
    bool has(Belief b) const;

    void ambient(int t);       // belief-agnostic cult activity (all scenarios)
    void torture(int t);
    void fear(int t);
    void breeding(int t);
    void chaos(int t);
    void sacrifice(int t);
    void conversion(int t);
    void war(int t);
    void reconstruction(int t);
    void trickery(int t);
    void magic(int t);
    void onslaught(int t);
    void dreams(int t);

    // Publish a death-cause event only if a cultist actually died, so the
    // "deaths by cause" metric stays 1:1 with real deaths.
    void deathEvent(EventType type, const std::string& causeTag);

    SimWorld& w_;
    std::vector<Belief> loadout_;
};

} // namespace sim
} // namespace cultulhu
