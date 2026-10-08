// Wave 9a: SimRunner — runs the scenario matrix (scenarios x seeds x fixed
// tick counts) headlessly and collects per-run balance metrics.

#pragma once

#include "beliefs/Belief.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cultulhu {
namespace sim {

struct SimScenario {
    std::string name;
    std::vector<Belief> loadout;
};

// The full scenario set: all 12 single-belief loadouts plus the six
// multi-belief loadouts under test.
std::vector<SimScenario> buildScenarios();

struct SimMetrics {
    std::string scenario;
    uint64_t seed = 0;

    std::vector<float> powerCurve; // power sampled every 100 ticks (t=0 first)
    float finalPower = 0.0f;
    int ticksTo500 = -1;           // -1 = never reached
    int ticksTo800 = -1;           // -1 = never reached

    int revolts = 0;               // Revolt events (insurrection boiled over)
    int riskEvents = 0;            // InsurrectionRiskUp events seen
    float maxRisk = 0.0f;

    int conversions = 0;           // souls converted (sum of amounts)
    int warKills = 0;              // EnemyCultistSlain events
    std::map<std::string, int> deathsByCause;

    float finalExertion[12] = {};

    // Deterministic serialization for the byte-identical determinism check.
    std::string serialize() const;
};

class SimRunner {
public:
    SimRunner(int ticks = 3600, int seedsPerScenario = 8,
              uint64_t seedBase = 0xC0015EEDULL);

    std::vector<SimMetrics> runAll();
    SimMetrics runOne(const SimScenario& sc, uint64_t seed);

    int ticks() const { return ticks_; }
    int seedsPerScenario() const { return seeds_; }

private:
    int ticks_;
    int seeds_;
    uint64_t seedBase_;
};

} // namespace sim
} // namespace cultulhu
