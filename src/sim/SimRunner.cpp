#include "sim/SimRunner.h"

#include "sim/ScenarioBot.h"
#include "sim/SimWorld.h"

#include <cstdio>
#include <sstream>

namespace cultulhu {
namespace sim {

namespace {
SimScenario single(Belief b) {
    SimScenario s;
    s.name = beliefName(b);
    s.loadout = {b};
    return s;
}

SimScenario triple(const char* name, Belief a, Belief b, Belief c) {
    SimScenario s;
    s.name = name;
    s.loadout = {a, b, c};
    return s;
}

SimScenario pair2(const char* name, Belief a, Belief b) {
    SimScenario s;
    s.name = name;
    s.loadout = {a, b};
    return s;
}
} // namespace

std::vector<SimScenario> buildScenarios() {
    std::vector<SimScenario> out;
    for (int i = 0; i < static_cast<int>(Belief::Count); ++i)
        out.push_back(single(static_cast<Belief>(i)));

    out.push_back(pair2("FearxTorture", Belief::Fear, Belief::Torture));
    out.push_back(pair2("WarxOnslaught", Belief::War, Belief::Onslaught));
    out.push_back(pair2("DreamsxChaos", Belief::Dreams, Belief::Chaos));
    out.push_back(triple("ConversionxTrickeryxMagic", Belief::Conversion,
                         Belief::Trickery, Belief::Magic));
    out.push_back(triple("SacrificexMagicxDreams", Belief::Sacrifice,
                         Belief::Magic, Belief::Dreams));
    out.push_back(triple("ChaosxReconstructionxWar", Belief::Chaos,
                         Belief::Reconstruction, Belief::War));
    return out;
}

std::string SimMetrics::serialize() const {
    std::ostringstream os;
    os << scenario << '|' << seed << '|';
    char buf[64];
    for (float p : powerCurve) {
        std::snprintf(buf, sizeof(buf), "%.4f,", p);
        os << buf;
    }
    os << '|';
    std::snprintf(buf, sizeof(buf), "%.4f", finalPower);
    os << buf << '|';
    os << ticksTo500 << '|' << ticksTo800 << '|';
    os << revolts << '|' << riskEvents << '|';
    std::snprintf(buf, sizeof(buf), "%.4f", maxRisk);
    os << buf << '|';
    os << conversions << '|' << warKills << '|';
    for (const auto& kv : deathsByCause) os << kv.first << '=' << kv.second << ';';
    os << '|';
    for (int i = 0; i < 12; ++i) {
        std::snprintf(buf, sizeof(buf), "%.4f,", finalExertion[i]);
        os << buf;
    }
    return os.str();
}

SimRunner::SimRunner(int ticks, int seedsPerScenario, uint64_t seedBase)
    : ticks_(ticks), seeds_(seedsPerScenario), seedBase_(seedBase) {}

SimMetrics SimRunner::runOne(const SimScenario& sc, uint64_t seed) {
    SimWorld world(seed, sc.loadout);
    ScenarioBot bot(world, sc.loadout);

    SimMetrics m;
    m.scenario = sc.name;
    m.seed = seed;
    m.powerCurve.reserve(static_cast<size_t>(ticks_) / 100 + 1);

    // Metric taps: count what the systems publish while the bot acts.
    world.bus().subscribe(EventType::Revolt,
                          [&](const GameEvent&) { ++m.revolts; });
    world.bus().subscribe(EventType::InsurrectionRiskUp,
                          [&](const GameEvent&) { ++m.riskEvents; });
    world.bus().subscribe(EventType::ConversionPerformed,
                          [&](const GameEvent& e) {
                              m.conversions += static_cast<int>(e.amount);
                          });
    world.bus().subscribe(EventType::EnemyCultistSlain,
                          [&](const GameEvent&) { ++m.warKills; });
    world.bus().subscribe(EventType::CultistOneHitKilled,
                          [&](const GameEvent&) { ++m.deathsByCause["one_hit"]; });
    world.bus().subscribe(EventType::CultistKilledByFeral,
                          [&](const GameEvent&) { ++m.deathsByCause["feral"]; });
    world.bus().subscribe(EventType::CultistLost,
                          [&](const GameEvent&) { ++m.deathsByCause["lost"]; });
    world.bus().subscribe(EventType::SacrificeCompleted,
                          [&](const GameEvent& e) {
                              if (e.tag == "sacrificed")
                                  ++m.deathsByCause["sacrificed"];
                          });

    m.powerCurve.push_back(world.power().value());
    for (int t = 0; t < ticks_; ++t) {
        bot.tick(t);
        world.tick(1.0);

        const float p = world.power().value();
        const float risk = world.cult().insurrectionRisk();
        if (risk > m.maxRisk) m.maxRisk = risk;
        if (m.ticksTo500 < 0 && p >= 500.0f) m.ticksTo500 = t + 1;
        if (m.ticksTo800 < 0 && p >= 800.0f) m.ticksTo800 = t + 1;
        if ((t + 1) % 100 == 0) m.powerCurve.push_back(p);
    }

    m.finalPower = world.power().value();
    for (int i = 0; i < 12; ++i)
        m.finalExertion[i] = world.exertion().exertion(static_cast<Belief>(i));
    return m;
}

std::vector<SimMetrics> SimRunner::runAll() {
    std::vector<SimMetrics> out;
    const std::vector<SimScenario> scenarios = buildScenarios();
    out.reserve(scenarios.size() * static_cast<size_t>(seeds_));
    for (const auto& sc : scenarios)
        for (int s = 0; s < seeds_; ++s)
            out.push_back(runOne(sc, seedBase_ + static_cast<uint64_t>(s) * 7919ULL));
    return out;
}

} // namespace sim
} // namespace cultulhu
