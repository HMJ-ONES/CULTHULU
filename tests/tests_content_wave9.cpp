// CULT-ULHU wave 9c tests: new ambient cultist events + dungeon hazards.
// Covers:
//   - the five new ambient actions (OmenReading, Sparring, TendWounded,
//     Graffiti, ChantingCircle): events published, effects, exertion
//     feeds, Dreams/Reconstruction synergies, zone fear, weighting
//   - dungeon hazards: spike-pit damage (once per entity per room),
//     hidden-trap trigger (once ever + Trickery feed), cave-in damage +
//     passage sealing, traverseTo guards, seeded determinism
// Compile manually (CMakeLists.txt is untouched):
//   g++ -std=c++17 -Isrc tests/tests_content_wave9.cpp build/libcultulhu.a \
//       -o /tmp/wave9_content_test && /tmp/wave9_content_test

#include "ai/AmbientBehavior.h"
#include "beliefs/BeliefSystem.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "exertion/ExertionSystem.h"
#include "power/PowerSystem.h"
#include "world/Dungeon.h"
#include "world/WorldMap.h"
#include "world/Zone.h"

#include <cmath>
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
    size_t count(EventType t) const {
        size_t n = 0;
        for (const auto& e : events)
            if (e.type == t) ++n;
        return n;
    }
    const GameEvent& last(EventType t) const {
        for (size_t i = events.size(); i-- > 0;)
            if (events[i].type == t) return events[i];
        static GameEvent none;
        return none;
    }
};

static void adoptNow(BeliefSystem& bs, Belief b) {
    CHECK(bs.requestChange(b, Belief::Count));
    bs.update(BeliefSystem::ADOPTION_TIME + 1.0);
    CHECK(bs.isActive(b));
}

struct World {
    EventBus bus;
    GameClock clock;
    RNG rng{1234};
    PowerSystem power;
    BeliefSystem beliefs{bus, clock};
    CultManager cult{bus, clock, rng};
    ExertionSystem exertion{bus, beliefs, power, cult, rng};
    AmbientDirector ambient{bus, rng, beliefs, cult, 30.0};
    Recorder rec;

    World() { ambient.setExertion(&exertion); }

    void attachAll() {
        rec.attach(bus, EventType::OmenRead);
        rec.attach(bus, EventType::SparringHeld);
        rec.attach(bus, EventType::HealPerformed);
        rec.attach(bus, EventType::SigilPainted);
        rec.attach(bus, EventType::ChantingHeld);
        rec.attach(bus, EventType::CreatureAttracted);
        rec.attach(bus, EventType::SpikePitSprung);
        rec.attach(bus, EventType::TrapSprung);
        rec.attach(bus, EventType::CaveIn);
    }
};

// ---- OmenReading ----

static void test_omen_reading() {
    World w;
    w.attachAll();
    adoptNow(w.beliefs, Belief::Dreams);
    Cultist& c = w.cult.recruit();

    CHECK(w.ambient.forceAction(0, AmbientAction::OmenReading));
    CHECK(w.rec.count(EventType::OmenRead) == 1);
    CHECK_CLOSE(w.rec.last(EventType::OmenRead).amount, 1.0f, 1e-4f);
    CHECK(w.rec.last(EventType::OmenRead).sourceId == c.id());
    // Exertion feed: OmenRead -> Dreams +3 (active: full rate).
    CHECK_CLOSE(w.exertion.exertion(Belief::Dreams), 13.0f, 1e-4f);
}

static void test_omen_reading_dreams_synergy() {
    World w;
    w.attachAll();
    adoptNow(w.beliefs, Belief::Dreams);
    w.cult.recruit();

    w.exertion.addExertion(Belief::Dreams, 90.0f); // 10 -> 100
    CHECK(w.ambient.forceAction(0, AmbientAction::OmenReading));
    // Dreams burning hot (>= 50): bonus power in the event amount.
    CHECK_CLOSE(w.rec.last(EventType::OmenRead).amount, 2.5f, 1e-4f);
}

// ---- Sparring ----

static void test_sparring() {
    World w;
    w.attachAll();
    Cultist& a = w.cult.recruit();
    Cultist& b = w.cult.recruit();
    a.setDevotion(50.0f);
    b.setDevotion(50.0f);

    CHECK(w.ambient.forceAction(0, AmbientAction::Sparring));
    CHECK(w.rec.count(EventType::SparringHeld) == 1);
    const GameEvent& e = w.rec.last(EventType::SparringHeld);
    CHECK((e.sourceId == a.id() && e.targetId == b.id()) ||
          (e.sourceId == b.id() && e.targetId == a.id()));
    // Tiny loyalty bump for both fighters.
    CHECK_CLOSE(a.devotion(), 52.0f, 1e-4f);
    CHECK_CLOSE(b.devotion(), 52.0f, 1e-4f);
    // Exertion feeds: War +2, Onslaught +1 (inactive: half rate).
    CHECK_CLOSE(w.exertion.exertion(Belief::War), 11.0f, 1e-4f);
    CHECK_CLOSE(w.exertion.exertion(Belief::Onslaught), 10.5f, 1e-4f);
}

static void test_sparring_no_partner() {
    World w;
    w.attachAll();
    w.cult.recruit(); // alone: nobody to spar with
    CHECK(w.ambient.forceAction(0, AmbientAction::Sparring));
    CHECK(w.rec.count(EventType::SparringHeld) == 0);
}

// ---- TendWounded ----

static void test_tend_wounded() {
    World w;
    w.attachAll();
    w.cult.recruit();
    Cultist& patient = w.cult.recruit();
    patient.takeDamage(50.0f); // hp 100 -> 50
    patient.setDevotion(50.0f);

    CHECK(w.ambient.forceAction(0, AmbientAction::TendWounded));
    CHECK(w.rec.count(EventType::HealPerformed) == 1);
    const GameEvent& e = w.rec.last(EventType::HealPerformed);
    CHECK(e.targetId == patient.id());
    // Base dose 18 (Reconstruction exertion 10 < 50: no synergy).
    CHECK_CLOSE(e.amount, 18.0f, 1e-4f);
    CHECK_CLOSE(patient.hp(), 68.0f, 1e-4f);
    CHECK_CLOSE(patient.devotion(), 53.0f, 1e-4f); // loyalty bump
    // HealPerformed feeds Reconstruction +1 (inactive: half rate).
    CHECK_CLOSE(w.exertion.exertion(Belief::Reconstruction), 10.5f, 1e-4f);
}

static void test_tend_wounded_reconstruction_synergy() {
    World w;
    w.attachAll();
    adoptNow(w.beliefs, Belief::Reconstruction);
    w.cult.recruit();
    Cultist& patient = w.cult.recruit();
    patient.takeDamage(60.0f); // hp 100 -> 40

    w.exertion.addExertion(Belief::Reconstruction, 90.0f); // 10 -> 100
    CHECK(w.ambient.forceAction(0, AmbientAction::TendWounded));
    // Reconstruction burning hot: stronger dose (30 instead of 18).
    CHECK_CLOSE(w.rec.last(EventType::HealPerformed).amount, 30.0f, 1e-4f);
    CHECK_CLOSE(patient.hp(), 70.0f, 1e-4f);
}

static void test_tend_wounded_nobody_hurt() {
    World w;
    w.attachAll();
    w.cult.recruit();
    w.cult.recruit();
    CHECK(w.ambient.forceAction(0, AmbientAction::TendWounded));
    CHECK(w.rec.count(EventType::HealPerformed) == 0);
}

// ---- Graffiti ----

static void test_graffiti() {
    World w;
    w.attachAll();
    Cultist& c = w.cult.recruit();
    c.setPosition(Vec3{1.0f, 0.0f, 1.0f});

    WorldMap map("test");
    ZoneDef def;
    def.name = "plaza";
    def.min = Vec3{-10.0f, -10.0f, -10.0f};
    def.max = Vec3{10.0f, 10.0f, 10.0f};
    map.addZone(std::move(def));
    w.ambient.setWorldMap(&map);

    CHECK(w.ambient.forceAction(0, AmbientAction::Graffiti));
    CHECK(w.rec.count(EventType::SigilPainted) == 1);
    CHECK(w.rec.last(EventType::SigilPainted).sourceId == c.id());
    // Fear rises in the cultist's current zone.
    CHECK_CLOSE(map.zone(0).ambient().ambientFear, 8.0f, 1e-4f);
    // Fear exertion feed: SigilPainted -> Fear +2 (inactive: half rate).
    CHECK_CLOSE(w.exertion.exertion(Belief::Fear), 11.0f, 1e-4f);
}

static void test_graffiti_fear_belief() {
    World w;
    w.attachAll();
    adoptNow(w.beliefs, Belief::Fear);
    w.cult.recruit();
    CHECK(w.ambient.forceAction(0, AmbientAction::Graffiti));
    // The Fear belief's dread rises through SigilPainted.
    CHECK_CLOSE(w.beliefs.fearLevel(), 3.0f, 1e-4f);
}

// ---- ChantingCircle ----

static void test_chanting_circle() {
    World w;
    w.attachAll();
    w.cult.recruit();

    CHECK(w.ambient.forceAction(0, AmbientAction::ChantingCircle));
    CHECK(w.rec.count(EventType::ChantingHeld) == 1);
    // Magic exertion gain: +3 (inactive: half rate).
    CHECK_CLOSE(w.exertion.exertion(Belief::Magic), 11.5f, 1e-4f);

    // Small chance to attract a wild creature: force enough chants that
    // at least one lures something (seeded RNG: deterministic).
    for (int i = 0; i < 300 &&
         w.rec.count(EventType::CreatureAttracted) == 0; ++i)
        w.ambient.forceAction(0, AmbientAction::ChantingCircle);
    CHECK(w.rec.count(EventType::CreatureAttracted) >= 1);
    const std::string tag = w.rec.last(EventType::CreatureAttracted).tag;
    CHECK(tag == "ghoul" || tag == "deep one" || tag == "night-gaunt");
}

// ---- Weighting of the new actions ----

static void test_new_action_weights() {
    World w;
    // Dreams + Magic + Fear adopted (MAX_ACTIVE is 3): the new belief-
    // gated actions must all appear in a long seeded run.
    adoptNow(w.beliefs, Belief::Dreams);
    adoptNow(w.beliefs, Belief::Magic);
    adoptNow(w.beliefs, Belief::Fear);
    Cultist& c = w.cult.recruit();
    c.setDevotion(40.0f); // low-ish morale also boosts TendWounded

    int seen[5] = {0, 0, 0, 0, 0}; // omen, spar, tend, graffiti, chant
    for (int i = 0; i < 4000; ++i) {
        AmbientAction a = chooseAmbientAction(c, w.beliefs, 0.4f, 23.0, w.rng);
        switch (a) {
            case AmbientAction::OmenReading:    seen[0]++; break;
            case AmbientAction::Sparring:       seen[1]++; break;
            case AmbientAction::TendWounded:    seen[2]++; break;
            case AmbientAction::Graffiti:       seen[3]++; break;
            case AmbientAction::ChantingCircle: seen[4]++; break;
            default: break;
        }
    }
    for (int i = 0; i < 5; ++i) CHECK(seen[i] > 0);
}

// ---- forceAction guards ----

static void test_force_action_guards() {
    World w;
    CHECK(!w.ambient.forceAction(0, AmbientAction::Pray)); // no cultists
    CHECK(!w.ambient.forceRandom());
    w.cult.recruit();
    CHECK(!w.ambient.forceAction(7, AmbientAction::Pray)); // out of range
    Cultist& c = w.cult.recruit();
    c.takeDamage(10000.0f); // dead: not eligible
    CHECK(!c.alive());
    CHECK(!w.ambient.forceAction(1, AmbientAction::Pray));
    CHECK(w.ambient.forceRandom()); // the living one still works
}

// ---- Dungeon hazards ----

// First seed (deterministic search) whose layout contains a hazard of
// the given type.
static uint64_t findSeedWith(HazardType type) {
    for (uint64_t seed = 1; seed < 500; ++seed) {
        EventBus bus;
        DungeonInstance d(bus, 1, seed, Vec3{0, 0, 0});
        for (const auto& h : d.hazards())
            if (h.type == type) return seed;
    }
    return 0;
}

static int hazardRoom(const DungeonInstance& d, HazardType type) {
    for (const auto& h : d.hazards())
        if (h.type == type) return h.roomIndex;
    return -1;
}

struct HazardWorld {
    EventBus bus;
    GameClock clock;
    RNG rng{99};
    PowerSystem power;
    BeliefSystem beliefs{bus, clock};
    CultManager cult{bus, clock, rng};
    ExertionSystem exertion{bus, beliefs, power, cult, rng};
    Recorder rec;

    HazardWorld() {
        rec.attach(bus, EventType::SpikePitSprung);
        rec.attach(bus, EventType::TrapSprung);
        rec.attach(bus, EventType::CaveIn);
    }
};

static void test_spike_pit() {
    HazardWorld w;
    const uint64_t seed = findSeedWith(HazardType::SpikePit);
    CHECK(seed != 0);
    DungeonInstance d(w.bus, 1, seed, Vec3{0, 0, 0});
    const int room = hazardRoom(d, HazardType::SpikePit);
    CHECK(room > 0);

    // Damage flows through the event: the subscriber applies it.
    Cultist& c = w.cult.recruit();
    w.bus.subscribe(EventType::SpikePitSprung, [&c](const GameEvent& e) {
        if (e.sourceId == c.id()) c.takeDamage(e.amount);
    });

    d.registerSurfaceEntity(c.id());
    CHECK(d.enter(c.id()));
    CHECK(d.entityRoom(c.id()) == 0);

    CHECK(d.traverseTo(c.id(), room));
    CHECK(w.rec.count(EventType::SpikePitSprung) == 1);
    CHECK_CLOSE(w.rec.last(EventType::SpikePitSprung).amount,
                DungeonInstance::SPIKE_PIT_DAMAGE, 1e-4f);
    CHECK_CLOSE(c.hp(), c.maxHp() - DungeonInstance::SPIKE_PIT_DAMAGE, 1e-4f);

    // Once per entity per room: leave and come back, no second trigger.
    CHECK(d.traverseTo(c.id(), 0));
    CHECK(d.traverseTo(c.id(), room));
    CHECK(w.rec.count(EventType::SpikePitSprung) == 1);
    CHECK_CLOSE(c.hp(), c.maxHp() - DungeonInstance::SPIKE_PIT_DAMAGE, 1e-4f);
}

static void test_spike_pit_per_entity() {
    HazardWorld w;
    const uint64_t seed = findSeedWith(HazardType::SpikePit);
    DungeonInstance d(w.bus, 1, seed, Vec3{0, 0, 0});
    const int room = hazardRoom(d, HazardType::SpikePit);

    Cultist& a = w.cult.recruit();
    Cultist& b = w.cult.recruit();
    d.registerSurfaceEntity(a.id());
    d.registerSurfaceEntity(b.id());
    d.enter(a.id());
    d.enter(b.id());
    CHECK(d.traverseTo(a.id(), room));
    CHECK(d.traverseTo(b.id(), room)); // a different entity still triggers
    CHECK(w.rec.count(EventType::SpikePitSprung) == 2);
}

static void test_hidden_trap() {
    HazardWorld w;
    const uint64_t seed = findSeedWith(HazardType::Trapped);
    CHECK(seed != 0);
    DungeonInstance d(w.bus, 1, seed, Vec3{0, 0, 0});
    const int room = hazardRoom(d, HazardType::Trapped);
    CHECK(room > 0);

    Cultist& c = w.cult.recruit();
    w.bus.subscribe(EventType::TrapSprung, [&c](const GameEvent& e) {
        if (e.sourceId == c.id()) c.takeDamage(e.amount);
    });

    d.registerSurfaceEntity(c.id());
    d.enter(c.id());
    CHECK(d.traverseTo(c.id(), room));
    CHECK(w.rec.count(EventType::TrapSprung) == 1);
    const GameEvent& e = w.rec.last(EventType::TrapSprung);
    CHECK(e.tag == "dungeon_trap");
    CHECK_CLOSE(e.amount, DungeonInstance::DUNGEON_TRAP_DAMAGE, 1e-4f);
    CHECK_CLOSE(c.hp(), c.maxHp() - DungeonInstance::DUNGEON_TRAP_DAMAGE, 1e-4f);
    // Trickery exertion feed via the existing TrapSprung row (+6, half).
    CHECK_CLOSE(w.exertion.exertion(Belief::Trickery), 13.0f, 1e-4f);

    // Hidden trap fires once, ever.
    CHECK(d.traverseTo(c.id(), 0));
    CHECK(d.traverseTo(c.id(), room));
    CHECK(w.rec.count(EventType::TrapSprung) == 1);
}

static void test_cave_in() {
    HazardWorld w;
    const uint64_t seed = findSeedWith(HazardType::CaveIn);
    CHECK(seed != 0);
    DungeonInstance d(w.bus, 1, seed, Vec3{0, 0, 0});
    const int room = hazardRoom(d, HazardType::CaveIn);
    CHECK(room > 0);

    Cultist& c = w.cult.recruit();
    w.bus.subscribe(EventType::CaveIn, [&c](const GameEvent& e) {
        if (e.targetId == c.id()) c.takeDamage(e.amount);
    });

    d.registerSurfaceEntity(c.id());
    d.enter(c.id());

    // Cave-ins roll per traversal: shuttle until one fires (seeded RNG:
    // deterministic outcome, bounded loop as a guard).
    int rounds = 0;
    while (w.rec.count(EventType::CaveIn) == 0 && rounds < 60) {
        CHECK(d.traverseTo(c.id(), 0));
        CHECK(d.traverseTo(c.id(), room));
        ++rounds;
    }
    CHECK(w.rec.count(EventType::CaveIn) >= 1);
    const GameEvent& e = w.rec.last(EventType::CaveIn);
    CHECK(e.targetId == c.id());
    CHECK_CLOSE(e.amount, DungeonInstance::CAVE_IN_DAMAGE, 1e-4f);
    CHECK(e.tag == "sealed" || e.tag == "rubble");
    CHECK(c.hp() < c.maxHp()); // entities inside were damaged
    // CaveIn feeds Chaos +4 (inactive: half rate).
    CHECK_CLOSE(w.exertion.exertion(Belief::Chaos), 12.0f, 1e-4f);

    // Keep traversing until the passage seals (40% per trigger), then
    // entry must be refused.
    rounds = 0;
    while (!d.roomSealed(room) && rounds < 200) {
        d.traverseTo(c.id(), 0);
        d.traverseTo(c.id(), room);
        ++rounds;
    }
    CHECK(d.roomSealed(room));
    CHECK(!d.traverseTo(c.id(), room)); // sealed: blocked
}

static void test_traverse_guards() {
    HazardWorld w;
    DungeonInstance d(w.bus, 1, 7, Vec3{0, 0, 0});
    Cultist& c = w.cult.recruit();
    CHECK(!d.traverseTo(c.id(), 1)); // not inside
    CHECK(d.entityRoom(c.id()) == -1);
    d.registerSurfaceEntity(c.id());
    d.enter(c.id());
    CHECK(!d.traverseTo(c.id(), -1)); // bad room
    CHECK(!d.traverseTo(c.id(), 9999));
    CHECK(d.traverseTo(c.id(), 0)); // entrance room: fine
    d.exit(c.id());
    CHECK(d.entityRoom(c.id()) == -1); // room tracking cleared on exit
}

static void test_hazard_determinism() {
    // Same seed -> identical hazard layout, for both generator flavors.
    EventBus b1, b2, b3, b4;
    DungeonInstance a(b1, 1, 4242, Vec3{0, 0, 0});
    DungeonInstance b(b2, 1, 4242, Vec3{0, 0, 0});
    CHECK(a.hazards().size() == b.hazards().size());
    for (size_t i = 0; i < a.hazards().size(); ++i) {
        CHECK(a.hazards()[i].type == b.hazards()[i].type);
        CHECK(a.hazards()[i].roomIndex == b.hazards()[i].roomIndex);
        CHECK_CLOSE(a.hazards()[i].triggerChance,
                    b.hazards()[i].triggerChance, 1e-6f);
    }
    CaveInstance c(b3, 2, 4242, Vec3{0, 0, 0});
    CaveInstance e(b4, 2, 4242, Vec3{0, 0, 0});
    CHECK(c.hazards().size() == e.hazards().size());
    for (size_t i = 0; i < c.hazards().size(); ++i) {
        CHECK(c.hazards()[i].type == e.hazards()[i].type);
        CHECK(c.hazards()[i].roomIndex == e.hazards()[i].roomIndex);
    }
    // Hazards only ever sit on non-entrance rooms.
    for (const auto& h : a.hazards()) CHECK(h.roomIndex > 0);
    for (const auto& h : c.hazards()) CHECK(h.roomIndex > 0);
}

int main() {
    test_omen_reading();
    test_omen_reading_dreams_synergy();
    test_sparring();
    test_sparring_no_partner();
    test_tend_wounded();
    test_tend_wounded_reconstruction_synergy();
    test_tend_wounded_nobody_hurt();
    test_graffiti();
    test_graffiti_fear_belief();
    test_chanting_circle();
    test_new_action_weights();
    test_force_action_guards();
    test_spike_pit();
    test_spike_pit_per_entity();
    test_hidden_trap();
    test_cave_in();
    test_traverse_guards();
    test_hazard_determinism();

    std::cout << "wave9-content checks: " << checks << ", failures: "
              << failures << "\n";
    return failures == 0 ? 0 : 1;
}
