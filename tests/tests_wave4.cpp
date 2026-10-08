// CULT-ULHU wave 4 tests: belief exertion, interaction matrix, derived
// stats, directive feeds, ritual caster, obedience interplay.
// Run via ctest or directly.

#include "ai/RitualCaster.h"
#include "beliefs/Belief.h"
#include "beliefs/BeliefSystem.h"
#include "beliefs/InteractionMatrix.h"
#include "combat/Attacks.h"
#include "combat/CrowdControl.h"
#include "commands/CommandSystem.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "entities/Entity.h"
#include "entities/Units.h"
#include "exertion/ActionExertionTable.h"
#include "exertion/DerivedStats.h"
#include "exertion/ExertionSystem.h"
#include "power/PowerSystem.h"

#include <cmath>
#include <cstdio>
#include <iostream>
#include <vector>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

#define CHECK_CLOSE(a, b, eps) do { ++checks; if (std::fabs((a) - (b)) > (eps)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #a " (" << (a) \
    << ") != " #b " (" << (b) << ")\n"; } } while (0)

static int bi(Belief b) { return static_cast<int>(b); }

static void adoptNow(BeliefSystem& bs, Belief b) {
    CHECK(bs.requestChange(b, Belief::Count));
    bs.update(BeliefSystem::ADOPTION_TIME + 1.0);
    CHECK(bs.isActive(b));
}

struct Recorder {
    std::vector<GameEvent> events;
    void attach(EventBus& bus, EventType t) {
        bus.subscribe(t, [this](const GameEvent& e) { events.push_back(e); });
    }
    size_t count(EventType t) const {
        size_t n = 0;
        for (const auto& e : events)
            if (e.type == t) ++n;
        return n;
    }
};

struct Wave4World {
    EventBus bus;
    GameClock clock;
    RNG rng{4242};
    BeliefSystem beliefs{bus, clock};
    PowerSystem power;
    CultManager cult{bus, clock, rng};
    ExertionSystem exertion{bus, beliefs, power, cult, rng};
};

// 1. Exertion accumulates from the action table (half rate when inactive).
static void testAccumulation() {
    Wave4World w;
    GameEvent e(EventType::TorturePerformed);
    e.amount = 2.0f;
    w.bus.publish(e);
    CHECK_CLOSE(w.exertion.exertion(Belief::Torture), 14.0f, 0.01f); // 10+8*.5
    CHECK_CLOSE(w.exertion.exertion(Belief::Fear), 11.5f, 0.01f);    // 10+3*.5

    adoptNow(w.beliefs, Belief::Torture); // now active: full rate
    w.bus.publish(e);
    CHECK_CLOSE(w.exertion.exertion(Belief::Torture), 22.0f, 0.01f); // 14+8
}

// 2. Exertion decays toward baseline and clamps at [0,100].
static void testDecayAndClamp() {
    Wave4World w;
    w.exertion.addExertion(Belief::War, 100.0f); // inactive: 10+50=60
    CHECK_CLOSE(w.exertion.exertion(Belief::War), 60.0f, 0.01f);
    w.exertion.update(10.0); // -0.5 toward baseline (wave 10: decay 0.05/s)
    CHECK_CLOSE(w.exertion.exertion(Belief::War), 59.5f, 0.01f);

    w.exertion.addExertion(Belief::Torture, 1000.0f);
    CHECK_CLOSE(w.exertion.exertion(Belief::Torture), 100.0f, 0.01f);
    w.exertion.addExertion(Belief::Torture, -1000.0f);
    CHECK_CLOSE(w.exertion.exertion(Belief::Torture), 0.0f, 0.01f);
}

// 3. Terror synergy (Fear x Torture): power gains x1.25 when both > 25.
static void testTerrorSynergy() {
    Wave4World a, b;
    adoptNow(a.beliefs, Belief::Torture);
    adoptNow(b.beliefs, Belief::Torture);
    // Pump B's exertion directly (no events, so belief state stays equal).
    b.exertion.addExertion(Belief::Torture, 100.0f); // active: ->100
    b.exertion.addExertion(Belief::Fear, 100.0f);    // inactive: ->60

    GameEvent e(EventType::TorturePerformed);
    e.amount = 2.0f;
    const float p0a = a.power.value(), p0b = b.power.value();
    a.bus.publish(e);
    b.bus.publish(e);
    const float base = a.power.value() - p0a;
    CHECK(base > 0.0f);
    CHECK_CLOSE(b.power.value() - p0b, base * 1.25f, 0.01f);
}

// 4. Conflict suppression: gains halved when both conflict beliefs > 50.
static void testConflictSuppression() {
    Wave4World w;
    w.exertion.addExertion(Belief::Reconstruction, 100.0f); // ->60
    w.exertion.addExertion(Belief::Onslaught, 100.0f);      // ->60
    w.exertion.addExertion(Belief::Reconstruction, 20.0f); // +10 halved -> +5
    CHECK_CLOSE(w.exertion.exertion(Belief::Reconstruction), 65.0f, 0.01f);
}

// 5. Loyalty drift follows the creed.
static void testLoyaltyDrift() {
    Wave4World w;
    w.cult.recruit();
    w.cult.recruit();
    w.exertion.addExertion(Belief::Sacrifice, 100.0f);
    w.exertion.addExertion(Belief::Dreams, 100.0f);
    w.exertion.addExertion(Belief::Conversion, 100.0f);
    const float d0 = w.cult.at(0).devotion();
    w.exertion.update(10.0);
    CHECK(w.cult.at(0).devotion() > d0); // devotional drift up

    w.exertion.addExertion(Belief::Chaos, 200.0f); // ->100
    const float d1 = w.cult.at(0).devotion();
    w.exertion.update(10.0);
    CHECK(w.cult.at(0).devotion() < d1); // chaos drags it down
}

// 6. Derived-stat formulas and bounds.
static void testDerivedStats() {
    float ex[12] = {0.0f};
    DerivedStats s0 = computeDerivedStats(ex, 0.0f);
    CHECK_CLOSE(s0.conversionChance, 0.35f, 0.001f);
    CHECK_CLOSE(s0.combatPowerMult, 1.0f, 0.001f);

    for (int i = 0; i < 12; ++i) ex[i] = 100.0f;
    DerivedStats s1 = computeDerivedStats(ex, 0.0f);
    // 0.35+0.4+0.4+0.1 infiltration-0.1 sabotage = 1.15 -> clamp 0.95
    CHECK_CLOSE(s1.conversionChance, 0.95f, 0.001f);
    // 1+0.004*200+0.002*100+0.15 blood frenzy = 2.15
    CHECK_CLOSE(s1.combatPowerMult, 2.15f, 0.001f);
    // 0.01*300/100 - 0.02*100/100 = 0.01
    CHECK_CLOSE(s1.loyaltyDriftPerSec, 0.01f, 0.001f);

    // Bounds hold under risk pressure too.
    DerivedStats s2 = computeDerivedStats(ex, 1.0f);
    CHECK(s2.conversionChance >= 0.05f && s2.conversionChance <= 0.95f);
    CHECK(s2.loyaltyDriftPerSec < s1.loyaltyDriftPerSec); // risk drags down
}

// 7. Obedience interplay: a chaotic, lunatic cult refuses far more often.
static int countBadOutcomes(bool chaosWorld) {
    EventBus bus;
    GameClock clock;
    RNG rng{7};
    BeliefSystem beliefs{bus, clock};
    PowerSystem power;
    CultManager cult{bus, clock, rng};
    ExertionSystem exertion{bus, beliefs, power, cult, rng};
    for (int i = 0; i < 5; ++i) cult.recruit();
    if (chaosWorld) {
        adoptNow(beliefs, Belief::Chaos);
        for (size_t i = 0; i < cult.size(); ++i)
            cult.at(i).setState(CultistState::Lunatic);
        cult.addRisk(70.0f);
    }
    CommandSystem commands{bus, rng, beliefs, cult};
    int bad = 0;
    for (int i = 0; i < 30; ++i) {
        CommandResult r =
            commands.issueCommand(DirectiveType::GoToWar, Vec3{100, 0, 100});
        if (r.outcome != CommandOutcome::Obeyed) ++bad;
    }
    return bad;
}

static void testObedienceInterplay() {
    const int calm = countBadOutcomes(false);
    const int chaotic = countBadOutcomes(true);
    std::cout << "  (calm refusals=" << calm
              << ", chaotic refusals=" << chaotic << ")\n";
    CHECK(chaotic > calm);
    // Note: refusals themselves raise insurrection risk (+5 each), so even
    // a healthy cult degrades over 30 straight commands — a feedback loop,
    // not a bug. The chaotic cult still refuses far more.
    CHECK(calm <= 20);
}

// Wave 10 (R1): stacked beliefs dilute power income: 1x / 0.75x / 0.5x.
static void testStackedIncomeMultiplier() {
    Wave4World one, two, three;
    adoptNow(one.beliefs, Belief::War);
    adoptNow(two.beliefs, Belief::War);
    adoptNow(two.beliefs, Belief::Fear);
    adoptNow(three.beliefs, Belief::War);
    adoptNow(three.beliefs, Belief::Fear);
    adoptNow(three.beliefs, Belief::Magic);

    GameEvent e(EventType::EnemyCultistSlain);
    const float p0 = one.power.value(), p1 = two.power.value(),
                p2 = three.power.value();
    one.bus.publish(e); two.bus.publish(e); three.bus.publish(e);
    const float base = one.power.value() - p0; // War: 10.0 power
    CHECK(base > 0.0f);
    CHECK_CLOSE(two.power.value() - p1, base * 0.75f, 0.05f);
    CHECK_CLOSE(three.power.value() - p2, base * 0.5f, 0.05f);
}

// Wave 10 (R3): disaffected cultists (devotion < 20) stoke insurrection risk
// on their own — no punishment required.
static void testDisaffectedRiskTrickle() {
    Wave4World w;
    CHECK_CLOSE(w.cult.insurrectionRisk(), 0.0f, 1e-6f);
    w.cult.recruit().setDevotion(10.0f);
    w.cult.update(100.0);
    CHECK_CLOSE(w.cult.insurrectionRisk(), 20.0f, 0.01f); // 0.2/s x 100s
}

// Wave 10 (R4): Onslaught idle decay starts after 20 in-game minutes.
static void testOnslaughtIdleLimit() {
    CHECK_CLOSE(BeliefSystem::ONSLAUGHT_IDLE_LIMIT, 1200.0, 1e-6);
}

// Wave 10 (R5): Reconstruction has a power path (rebuilds and heals).
static void testReconstructionPowerPath() {
    Wave4World w;
    adoptNow(w.beliefs, Belief::Reconstruction);
    GameEvent rb(EventType::BuildingRebuilt);
    CHECK_CLOSE(w.beliefs.onEvent(rb), 2.0f, 1e-6f);
    GameEvent h(EventType::HealPerformed);
    CHECK_CLOSE(w.beliefs.onEvent(h), 0.5f, 1e-6f);
}

// 8. Belief tension events fire on hot conflicts, then cool down.
static void testTensionEvents() {
    Wave4World w;
    Recorder rec;
    rec.attach(w.bus, EventType::BeliefTension);
    rec.attach(w.bus, EventType::InsurrectionRiskUp);
    w.exertion.addExertion(Belief::Reconstruction, 140.0f); // ->80
    w.exertion.addExertion(Belief::Onslaught, 140.0f);      // ->80
    w.exertion.update(1.0);
    CHECK(rec.count(EventType::BeliefTension) == 1);
    CHECK(rec.count(EventType::InsurrectionRiskUp) >= 1);
    w.exertion.update(1.0);
    CHECK(rec.count(EventType::BeliefTension) == 1); // cooldown holds
}

// 9. Directive resolutions feed exertion (tag "Name/Outcome").
static void testDirectiveFeeds() {
    Wave4World w;
    GameEvent r(EventType::DirectiveResolved);
    r.tag = "RaidCity/Obeyed";
    w.bus.publish(r);
    CHECK_CLOSE(w.exertion.exertion(Belief::Fear), 14.0f, 0.01f); // 10+8*.5

    GameEvent r2(EventType::DirectiveResolved);
    r2.tag = "GoToWar/Refused";
    w.bus.publish(r2);
    CHECK_CLOSE(w.exertion.exertion(Belief::Chaos), 12.0f, 0.01f); // 10+4*.5
}

// 10. Interaction matrix relations and thresholds.
static void testInteractionMatrix() {
    CHECK(beliefRelation(Belief::Fear, Belief::Torture) ==
          BeliefRelation::Synergy);
    CHECK(beliefRelation(Belief::Torture, Belief::Fear) ==
          BeliefRelation::Synergy); // symmetric
    CHECK(beliefRelation(Belief::Reconstruction, Belief::Onslaught) ==
          BeliefRelation::Conflict);
    CHECK(beliefRelation(Belief::War, Belief::Magic) == BeliefRelation::None);
    CHECK(beliefRelation(Belief::War, Belief::War) == BeliefRelation::None);

    float ex[12] = {0.0f};
    ex[bi(Belief::Dreams)] = 60.0f;
    ex[bi(Belief::Chaos)] = 60.0f;
    CHECK(synergyActive(Belief::Dreams, Belief::Chaos, ex));
    ex[bi(Belief::Chaos)] = 20.0f;
    CHECK(!synergyActive(Belief::Dreams, Belief::Chaos, ex));
    // Threshold parity between the matrix queries and the exertion system.
    CHECK_CLOSE(ExertionSystem::SYNERGY_THRESHOLD, 25.0f, 0.001f);
    CHECK(synergyPairCount() == 10);
    CHECK(conflictPairCount() == 5);
}

// 11. Ambient ritual caster: sorcerers attempt ritual conversions.
static void testRitualCaster() {
    EventBus bus;
    GameClock clock;
    RNG rng{99};
    BeliefSystem beliefs{bus, clock};
    PowerSystem power;
    CultManager cult{bus, clock, rng};
    ExertionSystem exertion{bus, beliefs, power, cult, rng};
    Sorcerer sorc{FACTION_CTHULHU, Vec3{0, 0, 0}};
    Civilian civ{Vec3{50, 0, 0}};
    RitualCaster rc{bus, rng, beliefs, exertion};
    rc.setSorcerers({&sorc});
    rc.setCivilians({&civ});

    // Max Conversion exertion -> conversion chance 0.79.
    exertion.addExertion(Belief::Conversion, 180.0f);
    Recorder rec;
    rec.attach(bus, EventType::SpellCast);
    rec.attach(bus, EventType::ConversionPerformed);

    for (int i = 0; i < 20; ++i) {
        sorc.setMana(100.0f);
        rc.update(46.0);
    }
    CHECK(rc.ritualsAttempted() == 20);
    CHECK(rec.count(EventType::SpellCast) == 20);
    CHECK(rc.ritualsSucceeded() >= 1); // p=0.79/attempt, seeded
    CHECK(sorc.mana() < 100.0f);

    // Out of range: no attempts.
    Civilian far{Vec3{5000, 0, 0}};
    rc.setCivilians({&far});
    const uint64_t before = rc.ritualsAttempted();
    sorc.setMana(100.0f);
    rc.update(46.0);
    CHECK(rc.ritualsAttempted() == before);
}

// 12. Faction combat power actually scales damage.
static void testCombatPowerScaling() {
    Wave4World w;
    ActiveEffects fx;
    Civilian a{Vec3{0, 0, 0}}, b{Vec3{0, 0, 0}};
    const float d1 = strikeMelee(1, EntityType::Cultist, a, 10.0f, w.beliefs,
                                 w.bus, fx, EventType::CivilianSlain, 1.0f);
    const float d2 = strikeMelee(1, EntityType::Cultist, b, 10.0f, w.beliefs,
                                 w.bus, fx, EventType::CivilianSlain, 2.0f);
    CHECK(d1 > 0.0f);
    CHECK_CLOSE(d2, d1 * 2.0f, 0.01f);
}

int main() {
    std::cout << "CULT-ULHU wave 4 tests\n";
    testAccumulation();
    testDecayAndClamp();
    testTerrorSynergy();
    testConflictSuppression();
    testLoyaltyDrift();
    testDerivedStats();
    testObedienceInterplay();
    testTensionEvents();
    testStackedIncomeMultiplier();
    testDisaffectedRiskTrickle();
    testOnslaughtIdleLimit();
    testReconstructionPowerPath();
    testDirectiveFeeds();
    testInteractionMatrix();
    testRitualCaster();
    testCombatPowerScaling();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
