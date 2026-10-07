// CULT-ULHU wave 5b tests: directive follow-through executor and driver
// save/load round-trip. Run via ctest or directly.
//
// Note: this file is compiled manually until the integrator wires it into
// CMakeLists.txt:
//   g++ -std=c++17 -I src tests/tests_wave5b.cpp -o /tmp/tests_wave5b \
//       build/libcultulhu.a && /tmp/tests_wave5b

#include "beliefs/Belief.h"
#include "beliefs/BeliefSystem.h"
#include "commands/CommandSystem.h"
#include "commands/DirectiveExecutor.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "entities/Entity.h"
#include "entities/Units.h"
#include "exertion/ExertionSystem.h"
#include "power/PowerSystem.h"
#include "save/SaveSystem.h"

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

struct ExecWorld {
    EventBus bus;
    GameClock clock;
    RNG rng{777};
    CultManager cult{bus, clock, rng};
    ExecWorld() {
        cult.recruit();
        cult.recruit();
        cult.recruit();
    }
};

struct FullWorld {
    EventBus bus;
    GameClock clock;
    RNG rng{4242};
    BeliefSystem beliefs{bus, clock};
    PowerSystem power;
    CultManager cult{bus, clock, rng};
    ExertionSystem exertion{bus, beliefs, power, cult, rng};
};

static GameEvent resolvedEvent(DirectiveType d, CommandOutcome o,
                               float chance = 0.8f) {
    GameEvent e(EventType::DirectiveResolved);
    e.tag = std::string(directiveName(d)) + "/" + commandOutcomeName(o);
    e.amount = chance;
    return e;
}

// 1. Raid operation lifecycle: spawn from a synthetic DirectiveResolved,
// tick to completion, progress + completion events fire.
static void testRaidLifecycle() {
    ExecWorld w;
    DirectiveExecutor exec{w.bus, w.rng, w.cult};
    Recorder raids, prog, done;
    raids.attach(w.bus, EventType::RaidPerformed);
    prog.attach(w.bus, EventType::DirectiveProgress);
    done.attach(w.bus, EventType::DirectiveCompleted);

    w.bus.publish(resolvedEvent(DirectiveType::RaidCity,
                                CommandOutcome::Obeyed));
    CHECK(exec.activeCount() == 1);

    for (int i = 0; i < 10; ++i) exec.update(5.0); // 50s full duration
    CHECK(exec.activeCount() == 0);

    CHECK(raids.events.size() == 10);
    for (const auto& e : raids.events)
        CHECK(e.amount >= 0.1f && e.amount <= 0.3f);

    CHECK(prog.events.size() == 10);
    for (const auto& e : prog.events) {
        CHECK(e.tag == "RaidCity");
        CHECK(e.amount >= 0.0f && e.amount <= 1.0f);
    }
    CHECK_CLOSE(prog.events.back().amount, 1.0f, 1e-5f);

    CHECK(done.events.size() == 1);
    CHECK(done.events[0].tag == "RaidCity");
}

// 2. Refused / SparksInsurrection spawn nothing.
static void testRefusedSpawnsNothing() {
    ExecWorld w;
    DirectiveExecutor exec{w.bus, w.rng, w.cult};
    w.bus.publish(resolvedEvent(DirectiveType::GoToWar,
                                CommandOutcome::Refused));
    w.bus.publish(resolvedEvent(DirectiveType::GoToWar,
                                CommandOutcome::SparksInsurrection));
    w.bus.publish(resolvedEvent(DirectiveType::RaidCity,
                                CommandOutcome::Refused));
    exec.update(120.0);
    CHECK(exec.activeCount() == 0);
}

// 3. Malformed tags and unknown directives spawn nothing.
static void testMalformedTag() {
    ExecWorld w;
    DirectiveExecutor exec{w.bus, w.rng, w.cult};
    GameEvent a(EventType::DirectiveResolved);
    a.tag = "NoSuchDirective/Obeyed";
    w.bus.publish(a);
    GameEvent b(EventType::DirectiveResolved);
    b.tag = "RaidCity"; // no slash
    w.bus.publish(b);
    GameEvent c(EventType::DirectiveResolved);
    c.tag = "";
    w.bus.publish(c);
    exec.update(120.0);
    CHECK(exec.activeCount() == 0);
}

// 4. Operation cap: at most MAX_OPERATIONS concurrent.
static void testCap() {
    ExecWorld w;
    DirectiveExecutor exec{w.bus, w.rng, w.cult};
    for (int i = 0; i < 6; ++i)
        w.bus.publish(resolvedEvent(DirectiveType::RaidCity,
                                    CommandOutcome::Obeyed));
    CHECK(exec.activeCount() == DirectiveExecutor::MAX_OPERATIONS);
    exec.update(50.0); // all four run their full duration
    CHECK(exec.activeCount() == 0);
}

// 5. PartiallyObeyed halves magnitudes and durations.
static void testPartialHalves() {
    ExecWorld w;
    DirectiveExecutor exec{w.bus, w.rng, w.cult};
    Recorder conv, done;
    conv.attach(w.bus, EventType::ConversionPerformed);
    done.attach(w.bus, EventType::DirectiveCompleted);

    w.bus.publish(resolvedEvent(DirectiveType::ConvertCampaign,
                                CommandOutcome::PartiallyObeyed));
    exec.update(5.0);
    exec.update(5.0);
    CHECK(conv.events.size() == 2);
    exec.update(5.0); // 15s halved duration -> done
    CHECK(conv.events.size() == 3);
    float sum = 0.0f;
    for (const auto& e : conv.events) {
        CHECK_CLOSE(e.amount, 1.0f, 1e-5f); // halved: 1 soul per tick
        sum += e.amount;
    }
    CHECK(exec.activeCount() == 0);
    CHECK(done.events.size() == 1);
    CHECK_CLOSE(sum, 3.0f, 1e-5f);

    // Full obedience for contrast: 6 ticks of 1-2 souls over 30s.
    ExecWorld w2;
    DirectiveExecutor exec2{w2.bus, w2.rng, w2.cult};
    Recorder conv2;
    conv2.attach(w2.bus, EventType::ConversionPerformed);
    w2.bus.publish(resolvedEvent(DirectiveType::ConvertCampaign,
                                 CommandOutcome::Obeyed));
    for (int i = 0; i < 6; ++i) exec2.update(5.0);
    CHECK(conv2.events.size() == 6);
    float sum2 = 0.0f;
    for (const auto& e : conv2.events) {
        CHECK(e.amount >= 1.0f && e.amount <= 2.0f);
        sum2 += e.amount;
    }
    CHECK(exec2.activeCount() == 0);
    CHECK(sum2 > sum); // full obedience yields strictly more
}

// 6. War operation ticks feed the exertion pipeline (War exertion rises).
static void testWarFeedsExertion() {
    FullWorld w;
    DirectiveExecutor exec{w.bus, w.rng, w.cult};
    Recorder slain;
    slain.attach(w.bus, EventType::EnemyCultistSlain);

    w.bus.publish(resolvedEvent(DirectiveType::GoToWar,
                                CommandOutcome::Obeyed));
    for (int i = 0; i < 8; ++i) exec.update(5.0); // 40s
    CHECK(slain.events.size() == 8);
    for (const auto& e : slain.events)
        CHECK(e.amount >= 1.0f && e.amount <= 3.0f);
    CHECK(w.exertion.exertion(Belief::War) > 0.0f);
    CHECK(exec.activeCount() == 0);
}

// 7. Defend operation: defenseActive() window + progress + completion.
static void testDefend() {
    ExecWorld w;
    DirectiveExecutor exec{w.bus, w.rng, w.cult};
    Recorder prog, done;
    prog.attach(w.bus, EventType::DirectiveProgress);
    done.attach(w.bus, EventType::DirectiveCompleted);

    w.bus.publish(resolvedEvent(DirectiveType::Defend,
                                CommandOutcome::Obeyed));
    CHECK(exec.activeCount() == 1);
    CHECK(exec.defenseActive());
    exec.update(30.0); // half of the 60s window
    CHECK(exec.defenseActive());
    CHECK(exec.activeCount() == 1);
    exec.update(30.0);
    CHECK(!exec.defenseActive());
    CHECK(exec.activeCount() == 0);
    CHECK(done.events.size() == 1);
    CHECK(done.events[0].tag == "Defend");
    CHECK(prog.events.size() == 6); // one per 10s tick
}

// 8. Sacrifice operation: exactly one terminal event (completed or
// interrupted); DirectiveCompleted only fires on completion.
static void testSacrifice() {
    ExecWorld w;
    DirectiveExecutor exec{w.bus, w.rng, w.cult};
    Recorder completed, interrupted, done;
    completed.attach(w.bus, EventType::SacrificeCompleted);
    interrupted.attach(w.bus, EventType::SacrificeInterrupted);
    done.attach(w.bus, EventType::DirectiveCompleted);

    w.bus.publish(resolvedEvent(DirectiveType::MassSacrifice,
                                CommandOutcome::Obeyed));
    for (int i = 0; i < 6; ++i) exec.update(5.0); // 30s ritual
    CHECK(completed.events.size() + interrupted.events.size() == 1);
    CHECK(exec.activeCount() == 0);
    if (!completed.events.empty())
        CHECK(done.events.size() == 1);
    else
        CHECK(done.events.size() == 0);
}

// 9. Relic operation: completion fires DirectiveCompleted; the relic find
// itself is a 60% gamble (ArtifactTriggered, tag "relic").
static void testRelic() {
    ExecWorld w;
    DirectiveExecutor exec{w.bus, w.rng, w.cult};
    Recorder art, done;
    art.attach(w.bus, EventType::ArtifactTriggered);
    done.attach(w.bus, EventType::DirectiveCompleted);

    w.bus.publish(resolvedEvent(DirectiveType::GatherRelic,
                                CommandOutcome::Obeyed));
    for (int i = 0; i < 6; ++i) exec.update(5.0); // 30s travel
    CHECK(done.events.size() == 1);
    CHECK(done.events[0].tag == "GatherRelic");
    CHECK(art.events.size() <= 1);
    if (!art.events.empty()) CHECK(art.events[0].tag == "relic");
    CHECK(exec.activeCount() == 0);
}

// 10. Save/load round-trip through SaveSystem (the driver's save/load
// commands are thin wrappers over this contract).
static void testSaveLoadRoundTrip() {
    const std::string path = "/tmp/wave5b_roundtrip.sav";
    GameState s;
    s.clockTime = 123.5;
    s.power = 321.25f;
    s.activeBeliefs = {Belief::Torture, Belief::Fear};
    s.insurrectionRisk = 42.0f;
    GameState::EntityRec a;
    a.id = 7;
    a.type = static_cast<int>(EntityType::EldritchAvatar);
    a.faction = 0;
    a.pos = Vec3{1.0f, 2.0f, 3.0f};
    a.hp = 1500.0f;
    a.maxHp = 2000.0f;
    GameState::EntityRec b;
    b.id = 9;
    b.type = static_cast<int>(EntityType::Sorcerer);
    b.faction = 0;
    b.pos = Vec3{4.0f, 5.0f, 6.0f};
    b.hp = 80.0f;
    b.maxHp = 120.0f;
    s.entities = {a, b};

    CHECK(SaveSystem::save(s, path));
    GameState t;
    CHECK(SaveSystem::load(path, t));
    CHECK_CLOSE(t.clockTime, 123.5, 1e-9);
    CHECK_CLOSE(t.power, 321.25f, 1e-3f);
    CHECK(t.activeBeliefs.size() == 2);
    CHECK(t.activeBeliefs[0] == Belief::Torture);
    CHECK(t.activeBeliefs[1] == Belief::Fear);
    CHECK_CLOSE(t.insurrectionRisk, 42.0f, 1e-3f);
    CHECK(t.entities.size() == 2);
    CHECK(t.entities[0].id == 7);
    CHECK(t.entities[0].type == static_cast<int>(EntityType::EldritchAvatar));
    CHECK(t.entities[0].faction == 0);
    CHECK_CLOSE(t.entities[0].pos.x, 1.0f, 1e-5f);
    CHECK_CLOSE(t.entities[0].pos.y, 2.0f, 1e-5f);
    CHECK_CLOSE(t.entities[0].pos.z, 3.0f, 1e-5f);
    CHECK_CLOSE(t.entities[0].hp, 1500.0f, 1e-3f);
    CHECK_CLOSE(t.entities[0].maxHp, 2000.0f, 1e-3f);
    CHECK(t.entities[1].id == 9);
    CHECK(t.entities[1].type == static_cast<int>(EntityType::Sorcerer));
    CHECK_CLOSE(t.entities[1].hp, 80.0f, 1e-3f);

    // Loading a missing file fails cleanly.
    GameState u;
    CHECK(!SaveSystem::load("/tmp/wave5b_does_not_exist.sav", u));
}

// 11. BeliefSystem::restoreActive replaces the set wholesale (used by
// driver load).
static void testRestoreActive() {
    EventBus bus;
    GameClock clock;
    BeliefSystem bs{bus, clock};
    CHECK(bs.requestChange(Belief::Torture, Belief::Count));
    bs.update(BeliefSystem::ADOPTION_TIME + 1.0);
    CHECK(bs.isActive(Belief::Torture));
    CHECK(bs.requestChange(Belief::War, Belief::Count)); // pending

    bs.restoreActive({Belief::Fear, Belief::Chaos});
    CHECK(!bs.isActive(Belief::Torture));
    CHECK(bs.isActive(Belief::Fear));
    CHECK(bs.isActive(Belief::Chaos));
    CHECK(bs.active().size() == 2);
    CHECK(bs.adoptionRemaining(Belief::War) < 0.0); // pending dropped

    // Invalid entries are skipped, cap respected.
    bs.restoreActive({Belief::Count, Belief::Torture, Belief::Fear,
                      Belief::Chaos, Belief::War, Belief::Conversion});
    CHECK(bs.active().size() == BeliefSystem::MAX_ACTIVE);
    CHECK(bs.isActive(Belief::Torture));
}

// 12. CultManager::clear drops the roster and cancels campaigns (used by
// driver load).
static void testCultClear() {
    EventBus bus;
    GameClock clock;
    RNG rng{11};
    CultManager cult{bus, clock, rng};
    cult.recruit();
    cult.recruit();
    cult.startConversionCampaign(5);
    CHECK(cult.size() == 2);
    CHECK(cult.campaignActive());
    cult.clear();
    CHECK(cult.size() == 0);
    CHECK(!cult.campaignActive());
}

int main() {
    std::cout << "CULT-ULHU wave 5b tests\n";
    testRaidLifecycle();
    testRefusedSpawnsNothing();
    testMalformedTag();
    testCap();
    testPartialHalves();
    testWarFeedsExertion();
    testDefend();
    testSacrifice();
    testRelic();
    testSaveLoadRoundTrip();
    testRestoreActive();
    testCultClear();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
