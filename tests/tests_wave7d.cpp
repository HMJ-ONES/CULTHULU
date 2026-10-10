// CULT-ULHU wave 7d tests: "Wave of Domination" (Cthulhu Avatar RMB).
// Covers: state transitions (wave -> levitate -> launch/drop), Chaos
// immunity, max victims, slam/launch kill attribution to Onslaught,
// high-drop death vs gentle-drop survival, direct-stun 2.5 s duration,
// 7 s cooldown enforcement.

#include "characters/abilities/RmbAbility.h"
#include "characters/abilities/WaveOfDomination.h"
#include "beliefs/BeliefSystem.h"
#include "combat/CrowdControl.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "entities/Structures.h"
#include "entities/Units.h"
#include "power/PowerSystem.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;
#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)
#define CHECK_CLOSE(a, b, eps) do { ++checks; if (std::fabs((a) - (b)) > (eps)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #a " ~ " #b "\n"; } } while (0)

// Simple event counter helper.
struct Counter {
    int n = 0;
    void attach(EventBus& b, EventType t) {
        b.subscribe(t, [this](const GameEvent&) { ++n; });
    }
};

struct Fixture {
    EventBus bus;
    GameClock clock;
    BeliefSystem beliefs;
    ActiveEffects effects;
    RNG rng{1234};
    PowerSystem power;
    // Keep entities alive for the whole test.
    std::vector<std::unique_ptr<Entity>> owned;
    EldritchAvatar* caster = nullptr;

    Fixture() : beliefs(bus, clock) {
        auto av = std::make_unique<EldritchAvatar>(FACTION_CTHULHU,
                                                  Vec3{0, 0, 0}, power);
        caster = av.get();
        caster->setFacingYaw(0.0f); // forward = +x
        owned.push_back(std::move(av));
    }

    RmbContext ctx() {
        RmbContext c(bus, effects, beliefs, rng);
        c.caster = caster;
        c.casterYaw = 0.0f;
        for (auto& e : owned) c.entities.push_back(e.get());
        return c;
    }

    Civilian* addCivilian(float x, float z) {
        auto c = std::make_unique<Civilian>(Vec3{x, 0, z});
        Civilian* p = c.get();
        owned.push_back(std::move(c));
        return p;
    }
};

static void testWaveCatchesAndLevitates() {
    Fixture f;
    auto civ = f.addCivilian(10.0f, 0.5f);
    WaveOfDomination w;
    auto c = f.ctx();

    CHECK(w.ready());
    w.onPress(c);
    CHECK(w.phase() == WaveOfDomination::Phase::Wave);
    CHECK(!w.ready()); // cooldown started on cast

    // Step until the wavefront passes the civilian and reaches max range
    // (25 m at 12 m/s needs 125 ticks).
    for (int i = 0; i < 140; ++i) w.update(c, 1.0 / 60.0);
    CHECK(w.phase() == WaveOfDomination::Phase::Hold);
    CHECK(w.victimCount() == 1);
    // Victim floats at the anchor in front of the caster.
    CHECK(civ->position().y > 1.0f);
    CHECK(w.suggestedCasterState() == AnimationState::Levitate);
}

static void testChaosImmunity() {
    Fixture f;
    auto lun = std::make_unique<Cultist>(FACTION_CTHULHU, Vec3{10, 0, 0});
    lun->setState(CultistState::Lunatic); // Chaos-aligned madness
    f.owned.push_back(std::move(lun));
    auto loyal = std::make_unique<Cultist>(FACTION_CTHULHU, Vec3{10, 0, 1});
    f.owned.push_back(std::move(loyal));
    auto feral = std::make_unique<Monstrosity>(1, Vec3{10, 0, -1}, "spawn",
                                              true);
    f.owned.push_back(std::move(feral));

    CHECK(!WaveOfDomination::isSusceptible(
        static_cast<Cultist*>(f.owned[1].get()))); // lunatic immune
    CHECK(WaveOfDomination::isSusceptible(
        static_cast<Cultist*>(f.owned[2].get()))); // loyal caught
    CHECK(!WaveOfDomination::isSusceptible(
        static_cast<Monstrosity*>(f.owned[3].get()))); // feral immune

    WaveOfDomination w;
    auto c = f.ctx();
    w.onPress(c);
    for (int i = 0; i < 200; ++i) w.update(c, 1.0 / 60.0);
    // Only the loyal cultist is levitated.
    CHECK(w.victimCount() == 1);
}

static void testMaxVictims() {
    Fixture f;
    for (int i = 0; i < 8; ++i)
        f.addCivilian(10.0f + i * 0.3f, (i % 3 - 1) * 0.8f);
    WaveOfDomination w;
    auto c = f.ctx();
    w.onPress(c);
    for (int i = 0; i < 200; ++i) w.update(c, 1.0 / 60.0);
    CHECK(w.victimCount() == WaveOfDomination::kMaxVictims);
}

static void testSlamKillsAndFeedsOnslaught() {
    Fixture f;
    Counter slain, slammed;
    slain.attach(f.bus, EventType::CivilianSlain);
    slammed.attach(f.bus, EventType::VictimSlammed);

    auto civ = f.addCivilian(10.0f, 0.0f);
    auto wall = std::make_unique<Building>(1, Vec3{6, 0, 0}, 1000.0f);
    Building* wallp = wall.get();
    f.owned.push_back(std::move(wall));

    WaveOfDomination w;
    auto c = f.ctx();
    w.onPress(c);
    // Step until the wave finishes travelling (25 m); stop the moment it
    // transitions so the slam below happens on the following ticks.
    int guard = 0;
    while (w.phase() == WaveOfDomination::Phase::Wave && guard++ < 300)
        w.update(c, 1.0 / 60.0);
    CHECK(w.phase() == WaveOfDomination::Phase::Hold);

    // The wall at x=6 sits inside the building hit radius (2.5 m) of the
    // anchor (x ~= 3), so the held victim slams into it on its own.
    for (int i = 0; i < 120; ++i) w.update(c, 1.0 / 60.0);

    CHECK(!civ->alive());              // victim died in the slam
    CHECK(slammed.n == 1);
    CHECK(slain.n == 1);               // Onslaught feed
    CHECK(wallp->hp() < 1000.0f);      // building took slam damage
}

static void testLaunchKills() {
    Fixture f;
    Counter launched, slain;
    launched.attach(f.bus, EventType::VictimsLaunched);
    slain.attach(f.bus, EventType::CivilianSlain);

    f.addCivilian(10.0f, 0.0f);
    f.addCivilian(11.0f, 0.5f);
    auto wall = std::make_unique<Building>(1, Vec3{30, 0, 0}, 1000.0f);
    f.owned.push_back(std::move(wall));

    WaveOfDomination w;
    auto c = f.ctx();
    w.onPress(c);
    for (int i = 0; i < 140; ++i) w.update(c, 1.0 / 60.0);
    CHECK(w.phase() == WaveOfDomination::Phase::Hold);
    const size_t n = w.victimCount();
    CHECK(n > 0);

    w.onLeftClick(c); // hurl them
    CHECK(launched.n == 1);
    for (int i = 0; i < 600; ++i) w.update(c, 1.0 / 60.0);
    // Victims flew 30 m/s toward +x; the wall at x=30 stops them.
    CHECK(w.phase() == WaveOfDomination::Phase::Idle);
    CHECK(slain.n == static_cast<int>(n)); // all died -> Onslaught feed
}

static void testDropSurvivesAndStagger() {
    Fixture f;
    Counter dropped;
    dropped.attach(f.bus, EventType::VictimDropped);
    auto civ = f.addCivilian(10.0f, 0.0f);

    WaveOfDomination w;
    auto c = f.ctx();
    w.onPress(c);
    for (int i = 0; i < 140; ++i) w.update(c, 1.0 / 60.0);
    CHECK(w.phase() == WaveOfDomination::Phase::Hold);

    w.onRelease(c); // gentle drop from ~2 m
    CHECK(dropped.n == 1);
    CHECK(civ->alive()); // lives
    CHECK(f.effects.isStunned(civ->id())); // 1 s stagger
    CHECK(w.phase() == WaveOfDomination::Phase::Idle);
}

static void testLethalDropKills() {
    Fixture f;
    Counter dropped, slain;
    dropped.attach(f.bus, EventType::VictimDropped);
    slain.attach(f.bus, EventType::CivilianSlain);
    auto civ = f.addCivilian(10.0f, 0.0f);

    WaveOfDomination w;
    auto c = f.ctx();
    // High cliff: ground far below.
    c.groundHeight = [](Vec3) { return -20.0f; };
    w.onPress(c);
    for (int i = 0; i < 140; ++i) w.update(c, 1.0 / 60.0);
    w.onRelease(c);
    CHECK(!civ->alive()); // fell to death
    CHECK(dropped.n == 1);
    CHECK(slain.n == 1);  // counts toward Onslaught
}

static void testDirectStun() {
    Fixture f;
    Counter stunned;
    stunned.attach(f.bus, EventType::DirectStunApplied);
    auto adv = std::make_unique<Adventurer>(Vec3{5, 0, 0});
    Adventurer* ap = adv.get();
    f.owned.push_back(std::move(adv));

    WaveOfDomination w;
    auto c = f.ctx();
    c.targetedEntityId = ap->id();
    w.onPress(c);
    CHECK(stunned.n == 1);
    CHECK(f.effects.isStunned(ap->id()));
    CHECK(w.phase() == WaveOfDomination::Phase::Idle); // no wave mode
    CHECK(!w.ready()); // cooldown consumed
    // Stun lasts ~2.5 s: tick 2 s, still stunned; tick 1 s more, free.
    f.effects.tick(2.0);
    CHECK(f.effects.isStunned(ap->id()));
    f.effects.tick(1.0);
    CHECK(!f.effects.isStunned(ap->id()));
}

static void testCooldownEnforced() {
    Fixture f;
    f.addCivilian(10.0f, 0.0f);
    WaveOfDomination w;
    auto c = f.ctx();
    w.onPress(c);
    CHECK_CLOSE(w.cooldownRemaining(), WaveOfDomination::kCooldownSec, 0.01f);
    // Immediate recast ignored.
    w.onPress(c);
    CHECK(w.phase() == WaveOfDomination::Phase::Wave); // still first cast
    // Drop the victim, then wait out the 7 s cooldown.
    for (int i = 0; i < 140; ++i) w.update(c, 1.0 / 60.0);
    w.onRelease(c);
    CHECK(w.phase() == WaveOfDomination::Phase::Idle);
    for (int i = 0; i < 600; ++i) w.update(c, 1.0 / 60.0);
    CHECK(w.ready());
}

static void testFactoryAndRegistry() {
    auto a = createRmbAbility("wave_of_domination");
    CHECK(a != nullptr);
    CHECK(std::string(a->abilityId()) == "wave_of_domination");
    CHECK_CLOSE(a->cooldownSec(), 7.0f, 0.001f);
    auto b = createRmbAbility("nope_not_real");
    CHECK(b == nullptr);
}

int main() {
    testWaveCatchesAndLevitates();
    testChaosImmunity();
    testMaxVictims();
    testSlamKillsAndFeedsOnslaught();
    testLaunchKills();
    testDropSurvivesAndStagger();
    testLethalDropKills();
    testDirectStun();
    testCooldownEnforced();
    testFactoryAndRegistry();
    std::cout << "wave7d checks: " << checks << ", failures: " << failures
              << "\n";
    return failures == 0 ? 0 : 1;
}
