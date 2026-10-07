// CULT-ULHU wave-3 tests: city destruction, converted-deny-death, and chaos
// lunatic misbehavior. Run via ctest or directly.

#include "beliefs/BeliefSystem.h"
#include "chaos/LunaticSystem.h"
#include "city/CitySystem.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "entities/Units.h"

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
    const GameEvent* first(EventType t) const {
        for (const auto& e : events)
            if (e.type == t) return &e;
        return nullptr;
    }
};

// ---- city destruction ----

static void test_city_founding() {
    EventBus bus;
    CitySystem cities(bus);
    City& c = cities.foundCity("Innsmouth", Vec3());
    c.addDistrict("Harbor", 1000, 4, 1000.0f);
    CHECK(cities.cityCount() == 1);
    CHECK(c.districts().size() == 1);
    CHECK(c.population() == 1000);
    CHECK_CLOSE(c.totalRuin(), 0.0f, 1e-6f);
}

static void test_city_damage_ruin() {
    EventBus bus;
    CitySystem cities(bus);
    Recorder rec;
    rec.attach(bus, EventType::RaidPerformed);
    rec.attach(bus, EventType::CityBuildingDestroyed);

    City& c = cities.foundCity("Innsmouth", Vec3());
    c.addDistrict("Harbor", 1000, 4, 1000.0f);

    cities.damageBuilding(c, 0, 0, 500.0f); // half of one building
    CHECK_CLOSE(c.totalRuin(), 500.0f / 4000.0f, 1e-6f);
    CHECK(rec.count(EventType::RaidPerformed) == 1);
    CHECK_CLOSE(rec.first(EventType::RaidPerformed)->amount, 0.125f, 1e-6f);
    CHECK(rec.count(EventType::CityBuildingDestroyed) == 0); // still standing
}

static void test_city_destroy_building_casualties() {
    EventBus bus;
    CitySystem cities(bus);
    Recorder rec;
    rec.attach(bus, EventType::CityBuildingDestroyed);
    rec.attach(bus, EventType::CivilianSlain);

    City& c = cities.foundCity("Innsmouth", Vec3());
    c.addDistrict("Harbor", 1000, 4, 1000.0f);

    cities.damageBuilding(c, 0, 1, 1000.0f); // destroy one building
    CHECK(rec.count(EventType::CityBuildingDestroyed) == 1);
    CHECK_CLOSE(rec.first(EventType::CityBuildingDestroyed)->amount,
                0.25f, 1e-6f);
    // per-building share 250 * casualtyRate 0.5 = 125 dead
    CHECK(rec.count(EventType::CivilianSlain) == 1);
    CHECK_CLOSE(rec.first(EventType::CivilianSlain)->amount, 125.0f, 1e-6f);
    CHECK(c.population() == 875);
}

static void test_district_razed_once() {
    EventBus bus;
    CitySystem cities(bus);
    Recorder rec;
    rec.attach(bus, EventType::DistrictRazed);

    City& c = cities.foundCity("Innsmouth", Vec3());
    c.addDistrict("Harbor", 1000, 4, 1000.0f);

    for (size_t i = 0; i < 4; ++i)
        cities.damageBuilding(c, 0, i, 1000.0f);
    CHECK(rec.count(EventType::DistrictRazed) == 1);
    CHECK(c.districts()[0].razed());
    CHECK(c.population() == 0);
    // Damaging the rubble again must not re-announce.
    cities.damageBuilding(c, 0, 0, 100.0f);
    CHECK(rec.count(EventType::DistrictRazed) == 1);
}

static void test_fear_from_raid() {
    EventBus bus;
    GameClock clock;
    RNG rng(7);
    CitySystem cities(bus);
    BeliefSystem beliefs(bus, clock);
    adoptNow(beliefs, Belief::Fear);

    Recorder rec;
    rec.attach(bus, EventType::RaidPerformed);
    City& c = cities.foundCity("Innsmouth", Vec3());
    c.addDistrict("Harbor", 1000, 4, 1000.0f);
    cities.damageBuilding(c, 0, 0, 1000.0f); // ruin delta 0.25

    const GameEvent* raid = rec.first(EventType::RaidPerformed);
    CHECK(raid != nullptr);
    const float delta = beliefs.onEvent(*raid);
    CHECK_CLOSE(delta, 12.0f * raid->amount, 1e-5f); // +12 power x destruction
    CHECK_CLOSE(beliefs.fearLevel(), 20.0f * raid->amount, 1e-5f);
}

static void test_onslaught_from_casualties() {
    EventBus bus;
    GameClock clock;
    RNG rng(7);
    CitySystem cities(bus);
    BeliefSystem beliefs(bus, clock);
    adoptNow(beliefs, Belief::Onslaught);

    Recorder rec;
    rec.attach(bus, EventType::CivilianSlain);
    City& c = cities.foundCity("Innsmouth", Vec3());
    c.addDistrict("Harbor", 1000, 4, 1000.0f);
    cities.damageBuilding(c, 0, 0, 1000.0f); // 125 casualties

    const GameEvent* slain = rec.first(EventType::CivilianSlain);
    CHECK(slain != nullptr);
    const float delta = beliefs.onEvent(*slain);
    CHECK_CLOSE(delta, 3.0f * 125.0f, 1e-4f); // +3 per civilian slain
}

// ---- converted-deny-death ----

static void test_deny_death_no_volunteer() {
    EventBus bus;
    GameClock clock;
    RNG rng(7);
    CultManager cult(bus, clock, rng);

    Cultist& dying = cult.recruit();
    dying.takeDamage(dying.maxHp(), true);
    CHECK(!cult.denyDeath(dying)); // no converted volunteer available
    CHECK(!dying.alive());
}

static void test_deny_death_volunteer() {
    EventBus bus;
    GameClock clock;
    RNG rng(7);
    CultManager cult(bus, clock, rng);
    BeliefSystem beliefs(bus, clock);
    adoptNow(beliefs, Belief::Sacrifice);

    Cultist& dying = cult.recruit();
    Cultist& volunteer = cult.recruit();
    volunteer.setState(CultistState::Converted);

    Recorder rec;
    rec.attach(bus, EventType::DeathDenied);

    dying.takeDamage(dying.maxHp(), true);
    CHECK(cult.denyDeath(dying));
    CHECK(dying.alive());
    CHECK_CLOSE(dying.hp(), dying.maxHp() * 0.25f, 1e-5f);
    CHECK(!volunteer.alive()); // served as the sacrifice
    CHECK(rec.count(EventType::DeathDenied) == 1);

    // Sacrifice belief rewards the dedicated death.
    CHECK_CLOSE(beliefs.onEvent(rec.events[0]), 20.0f, 1e-5f);
}

static void test_deny_death_requires_dead_cultist() {
    EventBus bus;
    GameClock clock;
    RNG rng(7);
    CultManager cult(bus, clock, rng);

    Cultist& healthy = cult.recruit();
    Cultist& volunteer = cult.recruit();
    volunteer.setState(CultistState::Converted);
    CHECK(!cult.denyDeath(healthy)); // nothing to deny
    CHECK(healthy.alive());
    CHECK(volunteer.alive());
}

// ---- chaos lunatics ----

static LunaticSystem makeLunatics(EventBus& bus, RNG& rng,
                                 BeliefSystem& beliefs, CultManager& cult) {
    // Certain lunacy, fast misbehavior: deterministic in tests.
    return LunaticSystem(bus, rng, beliefs, cult, 1.0, 1.0);
}

static void test_lunatics_appear_with_chaos() {
    EventBus bus;
    GameClock clock;
    RNG rng(7);
    CultManager cult(bus, clock, rng);
    BeliefSystem beliefs(bus, clock);
    adoptNow(beliefs, Belief::Chaos);

    cult.recruit();
    cult.recruit();
    cult.recruit();
    LunaticSystem lun = makeLunatics(bus, rng, beliefs, cult);
    lun.update(1.0);
    CHECK(lun.lunaticCount() == 3);
}

static void test_no_lunatics_without_chaos() {
    EventBus bus;
    GameClock clock;
    RNG rng(7);
    CultManager cult(bus, clock, rng);
    BeliefSystem beliefs(bus, clock); // Chaos not active

    cult.recruit();
    LunaticSystem lun = makeLunatics(bus, rng, beliefs, cult);
    lun.update(10.0);
    CHECK(lun.lunaticCount() == 0);
}

static void test_lunatic_interrupts_sacrifice() {
    EventBus bus;
    GameClock clock;
    RNG rng(7);
    CultManager cult(bus, clock, rng);
    BeliefSystem beliefs(bus, clock);
    adoptNow(beliefs, Belief::Chaos);
    adoptNow(beliefs, Belief::Sacrifice);

    cult.recruit(); // becomes the lunatic
    LunaticSystem lun = makeLunatics(bus, rng, beliefs, cult);

    Recorder rec;
    rec.attach(bus, EventType::LunaticActed);
    rec.attach(bus, EventType::SacrificeInterrupted);

    lun.update(1.0); // snap + act (only Sacrifice to target)
    CHECK(lun.lunaticCount() == 1);
    CHECK(rec.count(EventType::LunaticActed) == 1);
    CHECK(rec.first(EventType::LunaticActed)->tag == "interruptedRitual");
    CHECK(rec.count(EventType::SacrificeInterrupted) == 1);
    // The sabotage costs power through the normal Sacrifice rule.
    CHECK_CLOSE(beliefs.onEvent(*rec.first(EventType::SacrificeInterrupted)),
                -15.0f, 1e-5f);
}

static void test_lunatic_steals_devotion() {
    EventBus bus;
    GameClock clock;
    RNG rng(7);
    CultManager cult(bus, clock, rng);
    BeliefSystem beliefs(bus, clock);
    adoptNow(beliefs, Belief::Chaos);
    adoptNow(beliefs, Belief::Conversion);

    cult.recruit(); // the lunatic (set manually: rate 0 keeps the victim loyal)
    cult.recruit(); // loyal victim
    cult.at(0).setState(CultistState::Lunatic);
    LunaticSystem lun(bus, rng, beliefs, cult, 0.0, 1.0);

    Recorder rec;
    rec.attach(bus, EventType::LunaticActed);
    lun.update(1.0);
    CHECK(rec.count(EventType::LunaticActed) == 1);
    CHECK(rec.first(EventType::LunaticActed)->tag == "stoleDevotion");

    int converted = 0;
    for (size_t i = 0; i < cult.size(); ++i)
        if (cult.at(i).state() == CultistState::Converted) ++converted;
    CHECK(converted == 1);
}

static void test_align_lunatic_raises_risk() {
    EventBus bus;
    GameClock clock;
    RNG rng(7);
    CultManager cult(bus, clock, rng);
    BeliefSystem beliefs(bus, clock);
    adoptNow(beliefs, Belief::Chaos);

    cult.recruit();
    LunaticSystem lun = makeLunatics(bus, rng, beliefs, cult);
    lun.update(1.0);
    CHECK(lun.lunaticCount() == 1);

    Recorder rec;
    rec.attach(bus, EventType::LunaticAligned);
    lun.alignLunatic(0);
    CHECK(rec.count(EventType::LunaticAligned) == 1);
    CHECK_CLOSE(cult.insurrectionRisk(), 10.0f, 1e-5f); // InsurrectionRiskUp
    CHECK(!cult.at(0).alive());
}

static void test_align_nonlunatic_noop() {
    EventBus bus;
    GameClock clock;
    RNG rng(7);
    CultManager cult(bus, clock, rng);
    BeliefSystem beliefs(bus, clock);
    adoptNow(beliefs, Belief::Chaos);

    cult.recruit(); // stays loyal (rate 0)
    LunaticSystem lun(bus, rng, beliefs, cult, 0.0, 1.0);

    Recorder rec;
    rec.attach(bus, EventType::LunaticAligned);
    lun.alignLunatic(0);
    CHECK(rec.count(EventType::LunaticAligned) == 0);
    CHECK(cult.at(0).alive());
    CHECK_CLOSE(cult.insurrectionRisk(), 0.0f, 1e-5f);
}

int main() {
    test_city_founding();
    test_city_damage_ruin();
    test_city_destroy_building_casualties();
    test_district_razed_once();
    test_fear_from_raid();
    test_onslaught_from_casualties();
    test_deny_death_no_volunteer();
    test_deny_death_volunteer();
    test_deny_death_requires_dead_cultist();
    test_lunatics_appear_with_chaos();
    test_no_lunatics_without_chaos();
    test_lunatic_interrupts_sacrifice();
    test_lunatic_steals_devotion();
    test_align_lunatic_raises_risk();
    test_align_nonlunatic_noop();

    std::cout << "wave3: " << checks << " checks, " << failures
              << " failures\n";
    return failures == 0 ? 0 : 1;
}
