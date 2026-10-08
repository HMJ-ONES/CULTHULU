// CULT-ULHU wave 9b tests: the three new directives (AssassinateProphet,
// BlightLand, GrandSummoning) — full lifecycles through the
// CommandSystem/DirectiveExecutor/exertion/power pipeline.
//
// Note: this file is compiled manually until the integrator wires it into
// CMakeLists.txt:
//   g++ -std=c++17 -I src tests/tests_directives_wave9.cpp -o /tmp/tests_wave9 \
//       build/libcultulhu.a && /tmp/tests_wave9

#include "beliefs/Belief.h"
#include "beliefs/BeliefSystem.h"
#include "commands/CommandSystem.h"
#include "commands/DirectiveExecutor.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "entities/Units.h"
#include "exertion/ExertionSystem.h"
#include "power/PowerSystem.h"
#include "world/WorldMap.h"
#include "world/Zone.h"

#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

#define CHECK_CLOSE(a, b, eps) do { ++checks; if (std::fabs((a) - (b)) > (eps)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #a " (" << (a) \
    << ") != " #b " (" << (b) << ")\n"; } } while (0)

struct Recorder {
    std::vector<GameEvent> events;
    void attach(EventBus& bus, EventType t) {
        bus.subscribe(t, [this](const GameEvent& e) { events.push_back(e); });
    }
};

struct Wave9World {
    EventBus bus;
    GameClock clock;
    RNG rng{777};
    BeliefSystem beliefs{bus, clock};
    PowerSystem power;
    CultManager cult{bus, clock, rng};
    ExertionSystem exertion{bus, beliefs, power, cult, rng};
    CommandSystem commands{bus, rng, beliefs, cult};
    DirectiveExecutor executor{bus, rng, cult};
    WorldMap map{"test"};

    explicit Wave9World(uint64_t seed = 777) : rng(seed) {
        for (int i = 0; i < 3; ++i) {
            Cultist& c = cult.recruit();
            c.setPosition(Vec3{static_cast<float>(i * 3), 0, 5});
            c.setDevotion(90.0f);
        }
        ZoneDef def;
        def.name = "TestZone";
        def.min = Vec3{-100, -50, -100};
        def.max = Vec3{100, 50, 100};
        map.addZone(std::move(def));

        DirectiveContext ctx;
        ctx.power = &power;
        ctx.exertion = &exertion;
        ctx.worldMap = &map;
        executor.setContext(ctx);
    }

    void adopt(Belief b) {
        if (beliefs.requestChange(b, Belief::Count))
            beliefs.update(BeliefSystem::ADOPTION_TIME + 1.0);
    }
};

static GameEvent resolvedEvent(DirectiveType d, CommandOutcome o,
                               float chance = 0.8f,
                               Vec3 pos = Vec3{0, 0, 0}, int faction = 1) {
    GameEvent e(EventType::DirectiveResolved);
    e.tag = std::string(directiveName(d)) + "/" + commandOutcomeName(o);
    e.amount = chance;
    e.pos = pos;
    e.faction = faction;
    return e;
}

// 1. Directive names for the three new directives.
static void testNewDirectiveNames() {
    CHECK(std::string(directiveName(DirectiveType::AssassinateProphet)) ==
          "AssassinateProphet");
    CHECK(std::string(directiveName(DirectiveType::BlightLand)) ==
          "BlightLand");
    CHECK(std::string(directiveName(DirectiveType::GrandSummoning)) ==
          "GrandSummoning");
}

// 2. Obeyed/refused directives feed their aligned belief's exertion
// (refused feeds Chaos).
static void testDirectiveExertionFeeds() {
    Wave9World w(99);
    const float t0 = w.exertion.exertion(Belief::Trickery);
    const float f0 = w.exertion.exertion(Belief::Fear);
    const float m0 = w.exertion.exertion(Belief::Magic);
    const float c0 = w.exertion.exertion(Belief::Chaos);
    w.bus.publish(
        resolvedEvent(DirectiveType::AssassinateProphet, CommandOutcome::Obeyed));
    w.bus.publish(
        resolvedEvent(DirectiveType::BlightLand, CommandOutcome::Obeyed));
    w.bus.publish(
        resolvedEvent(DirectiveType::GrandSummoning, CommandOutcome::Obeyed));
    w.bus.publish(
        resolvedEvent(DirectiveType::AssassinateProphet, CommandOutcome::Refused));
    CHECK(w.exertion.exertion(Belief::Trickery) > t0);
    CHECK(w.exertion.exertion(Belief::Fear) > f0);
    CHECK(w.exertion.exertion(Belief::Magic) > m0);
    CHECK(w.exertion.exertion(Belief::Chaos) > c0);
}

// 3. issueCommand carries the enemy faction onto DirectiveResolved.
static void testIssueCommandFaction() {
    Wave9World w(7);
    Recorder resolved;
    resolved.attach(w.bus, EventType::DirectiveResolved);
    w.commands.issueCommand(DirectiveType::AssassinateProphet,
                            Vec3{10, 0, 10}, 2);
    CHECK(!resolved.events.empty());
    CHECK(resolved.events[0].faction == 2);
}

struct AssassResult {
    bool killed = false;
    bool exposed = false;
    size_t progressEvents = 0;
};

// Run one full assassination infiltration with a fixed seed.
static AssassResult runAssassination(uint64_t seed, float trickeryExertion,
                                     Vec3 target) {
    Wave9World w(seed);
    // Inactive beliefs accumulate at half rate: double the top-up.
    w.exertion.addExertion(Belief::Trickery, trickeryExertion * 2.0f);
    Recorder killed, exposed, progress;
    killed.attach(w.bus, EventType::LeaderAssassinated);
    exposed.attach(w.bus, EventType::AssassinExposed);
    progress.attach(w.bus, EventType::DirectiveProgress);

    DirectiveContext ctx = w.executor.context();
    ctx.targetEntityId = 4242;
    w.executor.setContext(ctx);

    w.bus.publish(resolvedEvent(DirectiveType::AssassinateProphet,
                                CommandOutcome::Obeyed, 0.8f, target, 2));
    for (int i = 0; i < 40 && w.executor.activeCount() > 0; ++i) {
        w.executor.update(5.0);
        w.exertion.update(5.0);
    }
    AssassResult r;
    r.killed = !killed.events.empty();
    r.exposed = !exposed.events.empty();
    r.progressEvents = progress.events.size();
    return r;
}

// 4. AssassinateProphet success branch (seeded): the leader dies, the
// enemy's morale breaks, Fear/War exertion and power spike.
static void testAssassinateSuccess() {
    int winSeed = -1;
    for (int s = 1; s <= 60 && winSeed < 0; ++s) {
        AssassResult r = runAssassination(static_cast<uint64_t>(s), 100.0f,
                                         Vec3{0, 0, 0});
        if (r.killed && !r.exposed && r.progressEvents >= 1) winSeed = s;
    }
    CHECK(winSeed > 0);
    if (winSeed <= 0) return;

    // Re-run the winning seed with full observation (Fear + War adopted
    // so the power spike is measurable).
    Wave9World w(static_cast<uint64_t>(winSeed));
    w.adopt(Belief::Fear);
    w.adopt(Belief::War);
    w.exertion.addExertion(Belief::Trickery, 200.0f);
    Recorder killed, exposed, shock, war, progress;
    killed.attach(w.bus, EventType::LeaderAssassinated);
    exposed.attach(w.bus, EventType::AssassinExposed);
    shock.attach(w.bus, EventType::EnemyMoraleShocked);
    war.attach(w.bus, EventType::CombatStarted);
    progress.attach(w.bus, EventType::DirectiveProgress);
    DirectiveContext ctx = w.executor.context();
    ctx.targetEntityId = 4242;
    w.executor.setContext(ctx);

    const float fearBefore = w.exertion.exertion(Belief::Fear);
    const float warBefore = w.exertion.exertion(Belief::War);
    const float powerBefore = w.power.value();
    w.bus.publish(resolvedEvent(DirectiveType::AssassinateProphet,
                                CommandOutcome::Obeyed, 0.8f, Vec3{0, 0, 0},
                                2));
    CHECK(w.executor.activeCount() == 1); // issued -> operation spawned
    for (int i = 0; i < 40 && w.executor.activeCount() > 0; ++i) {
        w.executor.update(5.0);
        w.exertion.update(5.0);
    }
    CHECK(w.executor.activeCount() == 0); // operation retired
    CHECK(killed.events.size() == 1);
    CHECK(killed.events[0].sourceId == 4242);
    CHECK(killed.events[0].faction == 2);
    CHECK(exposed.events.empty()); // no failure on the success branch
    CHECK(war.events.empty());
    CHECK(shock.events.size() == 1); // enemy morale shock emitted
    CHECK(shock.events[0].faction == 2);
    CHECK_CLOSE(shock.events[0].amount, 120.0f, 1e-3f);
    CHECK(!progress.events.empty()); // progress along the way
    // Big Fear + power spike through the pipeline.
    CHECK(w.exertion.exertion(Belief::Fear) > fearBefore);
    CHECK(w.exertion.exertion(Belief::War) > warBefore);
    CHECK_CLOSE(w.power.value(), powerBefore + 30.0f + 20.0f, 1e-3f);
}

// 5. AssassinateProphet failure branch (seeded): the assassin is exposed,
// the target escapes, the cult fears retaliation, the enemy deity notices.
static void testAssassinateExposure() {
    int doomSeed = -1;
    for (int s = 1; s <= 60 && doomSeed < 0; ++s) {
        AssassResult r = runAssassination(static_cast<uint64_t>(s), 0.0f,
                                         Vec3{0, 0, 0});
        if (r.exposed && !r.killed) doomSeed = s;
    }
    CHECK(doomSeed > 0);
    if (doomSeed <= 0) return;

    Wave9World w(static_cast<uint64_t>(doomSeed));
    w.exertion.addExertion(Belief::Trickery, 0.0f);
    Recorder killed, exposed, risk, war;
    killed.attach(w.bus, EventType::LeaderAssassinated);
    exposed.attach(w.bus, EventType::AssassinExposed);
    risk.attach(w.bus, EventType::InsurrectionRiskUp);
    war.attach(w.bus, EventType::CombatStarted);
    w.bus.publish(resolvedEvent(DirectiveType::AssassinateProphet,
                                CommandOutcome::Obeyed, 0.8f, Vec3{0, 0, 0},
                                2));
    for (int i = 0; i < 40 && w.executor.activeCount() > 0; ++i) {
        w.executor.update(5.0);
        w.exertion.update(5.0);
    }
    CHECK(w.executor.activeCount() == 0);
    CHECK(killed.events.empty()); // the target escapes: no kill
    CHECK(exposed.events.size() == 1);
    CHECK(exposed.events[0].sourceId != 0); // the infiltrator is named
    // Fear of retaliation: insurrection nudge among the cultists.
    bool foundNudge = false;
    for (const auto& e : risk.events)
        if (e.tag == "assassin_exposed" && e.amount == 4.0f) foundNudge = true;
    CHECK(foundNudge);
    CHECK(w.cult.insurrectionRisk() > 0.0f);
    // The enemy deity notices: a War event.
    bool foundWar = false;
    for (const auto& e : war.events)
        if (e.faction == 2 && e.tag == "deity_noticed") foundWar = true;
    CHECK(foundWar);
}

// 6. BlightLand full lifecycle: 120 corruption ticks, zone flagged Blighted.
static void testBlightLifecycle() {
    Wave9World w(1234);
    w.adopt(Belief::Fear);
    Recorder ticks, done, prog, blighted;
    ticks.attach(w.bus, EventType::ZoneBlightTick);
    done.attach(w.bus, EventType::DirectiveCompleted);
    prog.attach(w.bus, EventType::DirectiveProgress);
    blighted.attach(w.bus, EventType::ZoneBlighted);

    const float fearBefore = w.exertion.exertion(Belief::Fear);
    const float powerBefore = w.power.value();
    w.bus.publish(resolvedEvent(DirectiveType::BlightLand,
                                CommandOutcome::Obeyed, 0.8f,
                                Vec3{10, 0, 10}, 1));
    CHECK(w.executor.activeCount() == 1);
    for (int i = 0; i < 130 && w.executor.activeCount() > 0; ++i) {
        w.executor.update(1.0);
        w.exertion.update(1.0);
    }
    CHECK(w.executor.activeCount() == 0);
    CHECK(ticks.events.size() == 120); // ~120 corruption ticks
    CHECK(!prog.events.empty());
    CHECK(done.events.size() == 1);
    CHECK(done.events[0].tag == "BlightLand");
    // The city system's read-model: every tick names the zone with rising
    // progress.
    CHECK(ticks.events[0].tag == "TestZone");
    CHECK(ticks.events.back().amount > ticks.events.front().amount);
    // Completion flags the zone Blighted (persistent state on the zone).
    CHECK(blighted.events.size() == 1);
    CHECK(blighted.events[0].tag == "TestZone");
    CHECK(blighted.events[0].amount == 1.0f);
    CHECK(w.map.zone(0).blighted());
    // Fear exertion rises each tick; cultists in the zone trickle power.
    CHECK(w.exertion.exertion(Belief::Fear) > fearBefore);
    CHECK(w.power.value() > powerBefore);
}

// 7. BlightLand partial obedience: half the ticks, still flags the zone.
static void testBlightPartial() {
    Wave9World w(555);
    Recorder ticks, blighted;
    ticks.attach(w.bus, EventType::ZoneBlightTick);
    blighted.attach(w.bus, EventType::ZoneBlighted);
    w.bus.publish(resolvedEvent(DirectiveType::BlightLand,
                                CommandOutcome::PartiallyObeyed, 0.5f,
                                Vec3{10, 0, 10}, 1));
    for (int i = 0; i < 70 && w.executor.activeCount() > 0; ++i)
        w.executor.update(1.0);
    CHECK(w.executor.activeCount() == 0);
    CHECK(ticks.events.size() == 60);
    CHECK(blighted.events.size() == 1);
    CHECK(w.map.zone(0).blighted());
}

// 8. BlightLand refused: no operation spawns.
static void testBlightRefused() {
    Wave9World w(11);
    Recorder ticks, blighted;
    ticks.attach(w.bus, EventType::ZoneBlightTick);
    blighted.attach(w.bus, EventType::ZoneBlighted);
    w.bus.publish(resolvedEvent(DirectiveType::BlightLand,
                                CommandOutcome::Refused, 0.2f,
                                Vec3{10, 0, 10}, 1));
    w.executor.update(120.0);
    CHECK(ticks.events.empty());
    CHECK(blighted.events.empty());
    CHECK(!w.map.zone(0).blighted());
}

// 9. GrandSummoning: 300 power consumed immediately; fails when power < 300.
static void testSummonCostAndRefusal() {
    Wave9World w(42);
    w.power.set(500.0f);
    Recorder intr, champ;
    intr.attach(w.bus, EventType::SummoningInterrupted);
    champ.attach(w.bus, EventType::ChampionSummoned);
    w.bus.publish(resolvedEvent(DirectiveType::GrandSummoning,
                                CommandOutcome::Obeyed, 0.8f, Vec3{5, 0, 5},
                                1));
    CHECK_CLOSE(w.power.value(), 200.0f, 1e-3f); // 300 consumed at start
    CHECK(w.executor.activeCount() == 1);

    Wave9World poor(43);
    poor.power.set(299.0f);
    Recorder intr2, champ2;
    intr2.attach(poor.bus, EventType::SummoningInterrupted);
    champ2.attach(poor.bus, EventType::ChampionSummoned);
    poor.bus.publish(resolvedEvent(DirectiveType::GrandSummoning,
                                   CommandOutcome::Obeyed, 0.8f,
                                   Vec3{5, 0, 5}, 1));
    CHECK(intr2.events.size() == 1);
    CHECK(intr2.events[0].tag == "insufficient_power");
    CHECK(champ2.events.empty());
    CHECK_CLOSE(poor.power.value(), 299.0f, 1e-3f); // nothing consumed
    poor.executor.update(1.0);
    CHECK(poor.executor.activeCount() == 0);
}

// 10. GrandSummoning completion (seeded): the champion answers.
static void testSummonCompletion() {
    int cleanSeed = -1;
    for (int s = 1; s <= 60 && cleanSeed < 0; ++s) {
        Wave9World w(static_cast<uint64_t>(s));
        w.power.set(500.0f);
        Recorder champ, intr;
        champ.attach(w.bus, EventType::ChampionSummoned);
        intr.attach(w.bus, EventType::SummoningInterrupted);
        w.bus.publish(resolvedEvent(DirectiveType::GrandSummoning,
                                    CommandOutcome::Obeyed, 0.8f,
                                    Vec3{5, 0, 5}, 1));
        for (int i = 0; i < 30 && w.executor.activeCount() > 0; ++i)
            w.executor.update(5.0);
        if (!champ.events.empty() && intr.events.empty()) cleanSeed = s;
    }
    CHECK(cleanSeed > 0);
    if (cleanSeed <= 0) return;

    Wave9World w(static_cast<uint64_t>(cleanSeed));
    w.power.set(500.0f);
    Recorder champ, intr, done, prog;
    champ.attach(w.bus, EventType::ChampionSummoned);
    intr.attach(w.bus, EventType::SummoningInterrupted);
    done.attach(w.bus, EventType::DirectiveCompleted);
    prog.attach(w.bus, EventType::DirectiveProgress);
    const float magicBefore = w.exertion.exertion(Belief::Magic);
    w.bus.publish(resolvedEvent(DirectiveType::GrandSummoning,
                                CommandOutcome::Obeyed, 0.8f, Vec3{5, 0, 5},
                                1));
    for (int i = 0; i < 30 && w.executor.activeCount() > 0; ++i) {
        w.executor.update(5.0);
        w.exertion.update(5.0);
    }
    CHECK(w.executor.activeCount() == 0);
    CHECK(intr.events.empty());
    CHECK(champ.events.size() == 1);
    CHECK(champ.events[0].tag == "dread_champion");
    CHECK_CLOSE(champ.events[0].amount, 1200.0f, 1e-3f); // boosted Monstrosity
    CHECK_CLOSE(champ.events[0].pos.x, 5.0f, 1e-3f);
    CHECK(!prog.events.empty());
    CHECK(done.events.size() == 1);
    CHECK(done.events[0].tag == "GrandSummoning");
    CHECK_CLOSE(w.power.value(), 200.0f, 1e-3f); // spent, nothing refunded
    CHECK(w.exertion.exertion(Belief::Magic) > magicBefore);
}

// 11. GrandSummoning partial: a lesser champion (half HP).
static void testSummonPartial() {
    int cleanSeed = -1;
    for (int s = 1; s <= 60 && cleanSeed < 0; ++s) {
        Wave9World w(static_cast<uint64_t>(s));
        w.power.set(500.0f);
        Recorder champ, intr;
        champ.attach(w.bus, EventType::ChampionSummoned);
        intr.attach(w.bus, EventType::SummoningInterrupted);
        w.bus.publish(resolvedEvent(DirectiveType::GrandSummoning,
                                    CommandOutcome::PartiallyObeyed, 0.5f,
                                    Vec3{5, 0, 5}, 1));
        for (int i = 0; i < 30 && w.executor.activeCount() > 0; ++i)
            w.executor.update(5.0);
        if (!champ.events.empty() && intr.events.empty()) cleanSeed = s;
    }
    CHECK(cleanSeed > 0);
    if (cleanSeed <= 0) return;
    Wave9World w(static_cast<uint64_t>(cleanSeed));
    w.power.set(500.0f);
    Recorder champ;
    champ.attach(w.bus, EventType::ChampionSummoned);
    w.bus.publish(resolvedEvent(DirectiveType::GrandSummoning,
                                CommandOutcome::PartiallyObeyed, 0.5f,
                                Vec3{5, 0, 5}, 1));
    for (int i = 0; i < 30 && w.executor.activeCount() > 0; ++i)
        w.executor.update(5.0);
    CHECK(champ.events.size() == 1);
    CHECK_CLOSE(champ.events[0].amount, 600.0f, 1e-3f);
}

// 12. GrandSummoning interruption (seeded): power is NOT refunded.
static void testSummonInterruption() {
    int doomSeed = -1;
    for (int s = 1; s <= 60 && doomSeed < 0; ++s) {
        Wave9World w(static_cast<uint64_t>(s));
        w.power.set(500.0f);
        Recorder champ, intr;
        champ.attach(w.bus, EventType::ChampionSummoned);
        intr.attach(w.bus, EventType::SummoningInterrupted);
        w.bus.publish(resolvedEvent(DirectiveType::GrandSummoning,
                                    CommandOutcome::Obeyed, 0.8f,
                                    Vec3{5, 0, 5}, 1));
        for (int i = 0; i < 30 && w.executor.activeCount() > 0; ++i)
            w.executor.update(5.0);
        if (!intr.events.empty() && champ.events.empty() &&
            intr.events[0].tag != "insufficient_power")
            doomSeed = s;
    }
    CHECK(doomSeed > 0);
    if (doomSeed <= 0) return;

    Wave9World w(static_cast<uint64_t>(doomSeed));
    w.power.set(500.0f);
    Recorder champ, intr, done;
    champ.attach(w.bus, EventType::ChampionSummoned);
    intr.attach(w.bus, EventType::SummoningInterrupted);
    done.attach(w.bus, EventType::DirectiveCompleted);
    w.bus.publish(resolvedEvent(DirectiveType::GrandSummoning,
                                CommandOutcome::Obeyed, 0.8f, Vec3{5, 0, 5},
                                1));
    for (int i = 0; i < 30 && w.executor.activeCount() > 0; ++i)
        w.executor.update(5.0);
    CHECK(w.executor.activeCount() == 0);
    CHECK(champ.events.empty());
    CHECK(intr.events.size() == 1);
    CHECK(done.events.empty()); // interrupted: no completion event
    CHECK_CLOSE(w.power.value(), 200.0f, 1e-3f); // NOT refunded
}

// 13. End-to-end: issueCommand(BlightLand) with an obeying cult runs the
// whole directive lifecycle (issue -> progress -> completion).
static void testIssueBlightEndToEnd() {
    int obeySeed = -1;
    for (int s = 1; s <= 40 && obeySeed < 0; ++s) {
        Wave9World w(static_cast<uint64_t>(s));
        CommandResult r = w.commands.issueCommand(DirectiveType::BlightLand,
                                                  Vec3{10, 0, 10});
        if (r.outcome == CommandOutcome::Obeyed) obeySeed = s;
    }
    CHECK(obeySeed > 0);
    if (obeySeed <= 0) return;

    Wave9World w(static_cast<uint64_t>(obeySeed));
    Recorder issued, resolved, done, blighted;
    issued.attach(w.bus, EventType::DirectiveIssued);
    resolved.attach(w.bus, EventType::DirectiveResolved);
    done.attach(w.bus, EventType::DirectiveCompleted);
    blighted.attach(w.bus, EventType::ZoneBlighted);
    CommandResult r =
        w.commands.issueCommand(DirectiveType::BlightLand, Vec3{10, 0, 10});
    CHECK(r.outcome == CommandOutcome::Obeyed);
    CHECK(issued.events.size() == 1);
    CHECK(resolved.events.size() == 1);
    for (int i = 0; i < 130 && w.executor.activeCount() > 0; ++i) {
        w.executor.update(1.0);
        w.exertion.update(1.0);
    }
    CHECK(done.events.size() == 1);
    CHECK(blighted.events.size() == 1);
    CHECK(w.map.zone(0).blighted());
}

int main() {
    std::cout << "CULT-ULHU wave 9b directive tests\n";
    testNewDirectiveNames();
    testDirectiveExertionFeeds();
    testIssueCommandFaction();
    testAssassinateSuccess();
    testAssassinateExposure();
    testBlightLifecycle();
    testBlightPartial();
    testBlightRefused();
    testSummonCostAndRefusal();
    testSummonCompletion();
    testSummonPartial();
    testSummonInterruption();
    testIssueBlightEndToEnd();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
