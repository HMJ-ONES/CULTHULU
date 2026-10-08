// Wave 9a balance-sim tests. Built by hand (NOT in CMakeLists):
//   g++ -std=c++17 -O2 -Isrc tests/tests_sim.cpp build/libcultulhu.a \
//       -o /tmp/sim_tests && /tmp/sim_tests
// Covers: scenario-set shape, determinism (same seed+scenario ->
// byte-identical metrics), and run-to-completion without hangs.

#include "sim/ScenarioBot.h"
#include "sim/SimReporter.h"
#include "sim/SimRunner.h"
#include "sim/SimWorld.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>

using namespace cultulhu;
using namespace cultulhu::sim;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

static const SimScenario* findScenario(const std::vector<SimScenario>& v,
                                       const std::string& name) {
    for (const auto& s : v)
        if (s.name == name) return &s;
    return nullptr;
}

static void test_scenario_set_shape() {
    const auto scenarios = buildScenarios();
    CHECK(scenarios.size() == 18); // 12 singles + 6 multi-belief
    for (const auto& s : scenarios) {
        CHECK(!s.name.empty());
        CHECK(!s.loadout.empty());
        CHECK(s.loadout.size() <= 3); // max 3 active beliefs, per design
    }
    // Spot-check the required multi-belief loadouts exist.
    CHECK(findScenario(scenarios, "FearxTorture") != nullptr);
    CHECK(findScenario(scenarios, "WarxOnslaught") != nullptr);
    CHECK(findScenario(scenarios, "DreamsxChaos") != nullptr);
    CHECK(findScenario(scenarios, "ConversionxTrickeryxMagic") != nullptr);
    CHECK(findScenario(scenarios, "SacrificexMagicxDreams") != nullptr);
    CHECK(findScenario(scenarios, "ChaosxReconstructionxWar") != nullptr);
    const SimScenario* ft = findScenario(scenarios, "FearxTorture");
    CHECK(ft->loadout.size() == 2);
}

static void test_determinism() {
    const auto scenarios = buildScenarios();
    const SimScenario* sc = findScenario(scenarios, "Torture");
    CHECK(sc != nullptr);

    SimRunner runner(/*ticks=*/1200, /*seedsPerScenario=*/1);
    SimMetrics a = runner.runOne(*sc, 424242ULL);
    SimMetrics b = runner.runOne(*sc, 424242ULL);
    CHECK(a.serialize() == b.serialize()); // byte-identical

    // A different seed must still be reproducible with itself.
    SimMetrics c = runner.runOne(*sc, 777ULL);
    SimMetrics d = runner.runOne(*sc, 777ULL);
    CHECK(c.serialize() == d.serialize());
    CHECK(!a.serialize().empty());
}

static void test_run_to_completion() {
    const auto scenarios = buildScenarios();
    // One full-length run per scenario (single seed): must finish and
    // produce sane metrics.
    SimRunner runner(/*ticks=*/3600, /*seedsPerScenario=*/1);
    int done = 0;
    for (const auto& sc : scenarios) {
        SimMetrics m = runner.runOne(sc, 1000ULL + static_cast<uint64_t>(done));
        CHECK(m.powerCurve.size() == 37); // t=0 + 36 samples every 100 ticks
        CHECK(m.finalPower >= 0.0f && m.finalPower <= 1000.0f);
        CHECK(m.maxRisk >= 0.0f && m.maxRisk <= 100.0f);
        for (int i = 0; i < 12; ++i)
            CHECK(m.finalExertion[i] >= 0.0f && m.finalExertion[i] <= 100.0f);
        ++done;
    }
    CHECK(done == 18);
}

static void test_reporter_writes() {
    const auto scenarios = buildScenarios();
    SimRunner runner(/*ticks=*/300, /*seedsPerScenario=*/2);
    std::vector<SimMetrics> runs;
    for (size_t i = 0; i < 2; ++i)
        runs.push_back(runner.runOne(scenarios[i], 7ULL + i));

    SimReporter reporter("/tmp/sim_test_reports");
    reporter.writeAll(runs);

    std::ifstream r("/tmp/sim_test_reports/runs.csv");
    std::ifstream c("/tmp/sim_test_reports/power_curves.csv");
    std::ifstream s("/tmp/sim_test_reports/summary.csv");
    CHECK(r.good() && c.good() && s.good());
    std::string line;
    int runLines = 0;
    std::getline(r, line); // header
    while (std::getline(r, line)) ++runLines;
    CHECK(runLines == 2);
}

int main() {
    test_scenario_set_shape();
    test_determinism();
    test_run_to_completion();
    test_reporter_writes();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
