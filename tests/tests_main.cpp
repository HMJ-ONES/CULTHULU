// CULT-ULHU v0.1 sanity tests. Run via ctest or directly.
// Covers: power clamping, belief activation limits, adoption timers,
// punishment mechanics, belief power math, capture-point scoring,
// rituals, insurrection/revolts, and combat modifiers.

#include "beliefs/BeliefSystem.h"
#include "combat/Combat.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "entities/Structures.h"
#include "entities/Units.h"
#include "modes/CapturePointMode.h"
#include "modes/MobaDefense.h"
#include "power/PowerSystem.h"
#include "rituals/Ritual.h"

#include <cmath>
#include <iostream>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

#define CHECK_CLOSE(a, b, eps) do { ++checks; if (std::fabs((a) - (b)) > (eps)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #a " (" << (a) \
    << ") != " #b " (" << (b) << ")\n"; } } while (0)

// Activate a belief immediately by requesting it and fast-forwarding adoption.
static void adoptNow(BeliefSystem& bs, Belief b) {
    CHECK(bs.requestChange(b, Belief::Count));
    bs.update(BeliefSystem::ADOPTION_TIME + 1.0);
    CHECK(bs.isActive(b));
}

static void test_power_clamp() {
    PowerSystem p;
    CHECK_CLOSE(p.value(), 100.0f, 0.001f);
    p.add(5000.0f);
    CHECK_CLOSE(p.value(), 1000.0f, 0.001f);
    p.add(-9000.0f);
    CHECK_CLOSE(p.value(), 0.0f, 0.001f);
}

static void test_max_three_beliefs() {
    EventBus bus; GameClock clock; RNG rng(1);
    BeliefSystem bs(bus, clock);
    adoptNow(bs, Belief::Torture);
    adoptNow(bs, Belief::Fear);
    adoptNow(bs, Belief::War);
    CHECK(bs.active().size() == 3);
    // Fourth belief with no removal named: rejected.
    CHECK(!bs.requestChange(Belief::Magic, Belief::Count));
    // Swap works: War out, Magic in (pending, not yet active).
    CHECK(bs.requestChange(Belief::Magic, Belief::War));
    CHECK(!bs.isActive(Belief::Magic));
    CHECK(!bs.isActive(Belief::War));
    CHECK(bs.active().size() == 2);
}

static void test_adoption_timer() {
    EventBus bus; GameClock clock; RNG rng(2);
    BeliefSystem bs(bus, clock);
    CHECK(bs.requestChange(Belief::Breeding, Belief::Count));
    bs.update(60.0);
    CHECK(!bs.isActive(Belief::Breeding));       // not yet adopted
    CHECK(bs.adoptionRemaining(Belief::Breeding) > 0.0);
    bs.update(61.0);
    CHECK(bs.isActive(Belief::Breeding));        // adopted after ~120s
}

static void test_punish_shortens_adoption_and_raises_risk() {
    EventBus bus; GameClock clock; RNG rng(3);
    BeliefSystem bs(bus, clock);
    float riskAdded = 0.0f;
    bus.subscribe(EventType::InsurrectionRiskUp,
                  [&](const GameEvent& e) { riskAdded += e.amount; });
    CHECK(bs.requestChange(Belief::Sacrifice, Belief::Count));
    double before = bs.adoptionRemaining(Belief::Sacrifice);
    bs.punishInfringer();
    double after = bs.adoptionRemaining(Belief::Sacrifice);
    CHECK(after < before);                       // adoption accelerated
    CHECK_CLOSE(after, before * 0.5, 0.001);     // halved
    CHECK(riskAdded > 0.0f);                     // but insurrection risk rose
}

static void test_chaos_faster_adoption() {
    EventBus bus; GameClock clock; RNG rng(4);
    BeliefSystem bs(bus, clock);
    adoptNow(bs, Belief::Chaos);
    CHECK(bs.requestChange(Belief::Trickery, Belief::Count));
    bs.update(61.0); // 61s * 2x speed > 120s
    CHECK(bs.isActive(Belief::Trickery));
}

static void test_torture_power_math() {
    EventBus bus; GameClock clock; RNG rng(5);
    BeliefSystem bs(bus, clock);
    adoptNow(bs, Belief::Torture);

    GameEvent t(EventType::TorturePerformed); t.amount = 2.0f;
    CHECK_CLOSE(bs.onEvent(t), 16.0f, 0.001f);   // 2 victims * 8

    GameEvent c(EventType::CapturedTortured); c.amount = 1.0f;
    CHECK_CLOSE(bs.onEvent(c), 5.0f, 0.001f);

    GameEvent o(EventType::OwnCultistTortured);
    CHECK_CLOSE(bs.onEvent(o), 0.0f, 0.001f);    // no benefit, no risk

    GameEvent k(EventType::CultistOneHitKilled);
    CHECK(bs.onEvent(k) < 0.0f);                 // power loss
}

static void test_fear_decay() {
    EventBus bus; GameClock clock; RNG rng(6);
    BeliefSystem bs(bus, clock);
    adoptNow(bs, Belief::Fear);

    GameEvent raid(EventType::RaidPerformed); raid.amount = 1.0f;
    CHECK(bs.onEvent(raid) > 0.0f);
    CHECK(bs.fearLevel() > 0.0f);

    bs.setInCombat(false);
    float d = bs.tick(10.0);                    // 10s out of combat
    CHECK(d < 0.0f);                            // power decays with fear
    CHECK(bs.fearLevel() < 20.0f);
}

static void test_conversion_cooldown() {
    EventBus bus; GameClock clock; RNG rng(7);
    BeliefSystem bs(bus, clock);
    adoptNow(bs, Belief::Conversion);
    CHECK(bs.canMassConvert());
    int massEvents = 0;
    bus.subscribe(EventType::MassConversionUsed,
                  [&](const GameEvent&) { ++massEvents; });
    bs.useMassConvert();
    CHECK(!bs.canMassConvert());
    CHECK(massEvents == 1);
    clock.advance(199.0);
    CHECK(!bs.canMassConvert());
    clock.advance(2.0);
    CHECK(bs.canMassConvert());                 // 200s cooldown elapsed
}

static void test_war_scaling() {
    EventBus bus; GameClock clock; RNG rng(8);
    BeliefSystem bs(bus, clock);
    adoptNow(bs, Belief::War);

    GameEvent kill(EventType::EnemyCultistSlain);
    bs.setActiveWars(1);
    float singleWar = bs.onEvent(kill);
    bs.setActiveWars(3);
    float multiWar = bs.onEvent(kill);
    CHECK(singleWar > 0.0f);
    CHECK(multiWar > 0.0f);
    CHECK(multiWar < singleWar);                 // lower rate vs many deities

    GameEvent dmg(EventType::BaseBuildingDestroyed);
    CHECK(bs.onEvent(dmg) < 0.0f);
}

static void test_capture_point_scoring() {
    EventBus bus; GameClock clock; RNG rng(9);
    CapturePointMode mode(bus, clock);
    mode.addPoint(Vec3(0, 0, 0));
    mode.setOccupants(0, 3, 0);                 // team 0 holds the point
    mode.update(5.0);                           // 0.25 * 5 * 3 = 3.75 -> captured
    CHECK(mode.pointOwner(0) == 0);
    float s0 = mode.score(0);
    mode.update(30.0);                          // 6 scoring ticks
    CHECK(mode.score(0) > s0);
    CHECK(mode.score(1) == 0.0f);
    CHECK(!mode.isOver());

    // Contested point: no capture progress for the other team.
    mode.addPoint(Vec3(100, 0, 0));
    mode.setOccupants(1, 2, 2);
    mode.update(30.0);
    CHECK(mode.pointOwner(1) == -1);
}

static void test_sacrifice_ritual() {
    EventBus bus; GameClock clock; RNG rng(10);
    bool completed = false, interrupted = false;
    bus.subscribe(EventType::SacrificeCompleted, [&](const GameEvent&) { completed = true; });
    bus.subscribe(EventType::SacrificeInterrupted, [&](const GameEvent&) { interrupted = true; });

    SacrificeRitual r(bus);
    r.start();
    CHECK(!r.update(10.0));
    CHECK(r.active());
    CHECK_CLOSE(r.progress(), 10.0f / 30.0f, 0.01f);
    CHECK(r.update(25.0));                       // completes at 30s
    CHECK(completed);
    CHECK(!r.active());

    SacrificeRitual r2(bus);
    r2.start();
    r2.update(5.0);
    r2.interrupt();
    CHECK(interrupted);
    CHECK(r2.wasInterrupted());
}

static void test_insurrection_revolt() {
    EventBus bus; GameClock clock; RNG rng(11);
    CultManager cult(bus, clock, rng);
    bool revolted = false;
    bus.subscribe(EventType::Revolt, [&](const GameEvent&) { revolted = true; });
    cult.recruit(); cult.recruit();
    CHECK(cult.size() == 2);
    cult.addRisk(85.0f);
    CHECK(cult.update(1.0));                     // revolt triggers
    CHECK(revolted);
    CHECK(cult.insurrectionRisk() < 80.0f);       // risk burned off
}

static void test_onslaught_melee_bonus() {
    EventBus bus; GameClock clock; RNG rng(12);
    BeliefSystem plain(bus, clock), buffed(bus, clock);
    adoptNow(buffed, Belief::Onslaught);

    AttackInfo atk;
    atk.baseDamage = 40.0f;
    atk.melee = true;
    float base = combat::calcDamage(atk, plain);
    float boosted = combat::calcDamage(atk, buffed);
    CHECK_CLOSE(base, 40.0f, 0.001f);
    CHECK_CLOSE(boosted, 50.0f, 0.001f);         // 1.25x melee bonus
}

static void test_magic_cc_reduction() {
    EventBus bus; GameClock clock; RNG rng(13);
    BeliefSystem plain(bus, clock), buffed(bus, clock);
    adoptNow(buffed, Belief::Magic);
    CHECK_CLOSE(combat::ccDuration(10.0f, plain), 10.0f, 0.001f);
    CHECK_CLOSE(combat::ccDuration(10.0f, buffed), 6.0f, 0.001f); // 0.6x
}

static void test_moba_basics() {
    EventBus bus; GameClock clock; RNG rng(14);
    MobaDefense moba(bus, clock, rng);
    moba.addLane({Vec3(0, 0, 0), Vec3(50, 0, 0), Vec3(100, 0, 0)});
    moba.setBase(0, Vec3(0, 0, 0), 500.0f);
    moba.setBase(1, Vec3(100, 0, 0), 500.0f);
    CHECK(moba.laneCount() == 1);
    moba.spawnWave(0, 0);
    CHECK(moba.minionCount() == 4);
    moba.update(1.0);
    CHECK(!moba.isOver());
    CHECK(moba.winner() == -1);
}

static void test_entity_damage() {
    Cultist c(FACTION_CTHULHU, Vec3());
    CHECK(c.alive());
    CHECK(!c.takeDamage(30.0f));
    CHECK_CLOSE(c.hp(), 70.0f, 0.001f);
    // One-hit kill detection: a single blow on a healthy target.
    Cultist c2(FACTION_CTHULHU, Vec3());
    CHECK(c2.takeDamage(200.0f, true));
    CHECK(c2.wasOneHitKilled());
    // Not a one-hit kill if the target was already damaged.
    Cultist c3(FACTION_CTHULHU, Vec3());
    c3.takeDamage(10.0f);
    c3.takeDamage(200.0f, true);
    CHECK(!c3.wasOneHitKilled());
}

int main() {
    test_power_clamp();
    test_max_three_beliefs();
    test_adoption_timer();
    test_punish_shortens_adoption_and_raises_risk();
    test_chaos_faster_adoption();
    test_torture_power_math();
    test_fear_decay();
    test_conversion_cooldown();
    test_war_scaling();
    test_capture_point_scoring();
    test_sacrifice_ritual();
    test_insurrection_revolt();
    test_onslaught_melee_bonus();
    test_magic_cc_reduction();
    test_moba_basics();
    test_entity_damage();

    std::cout << "checks: " << checks << ", failures: " << failures << "\n";
    if (failures == 0) std::cout << "ALL TESTS PASSED\n";
    return failures == 0 ? 0 : 1;
}
