// CULT-ULHU wave 13 tests: the Vale of Pnath.
//
// Covers: seeded generation determinism (same seed -> same rooms, depths,
// hazards); depth assignment (room 0 = 0, deepest > 0); dreadAt scaling
// (0 at mouth, 1.0 at deepest); hazard triggers via traverseTo —
// AbyssPit (damage scales with depth, once per entity), Whispers
// (fear scales with dread, fires every traversal), DholeTunnel
// (first traversal = DholeTremors telegraph, second = DholeAmbush);
// Dhole entity (species, HP, burrowed default, fear aura); dhole model
// slot is the intentional "" placeholder; relic vault / guardian spawn
// positions are valid; Vale is bigger than a base dungeon.
//
// Legal: asserts no forbidden Mythos terms appear in the new strings
// (Hounds of Tindalos etc. — see assets/creatures/LEGAL_NAMES.md).

#include "assets/ModelCatalog.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "entities/Units.h"
#include "world/ValeOfPnath.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

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

static void attachVale(Recorder& r, EventBus& bus) {
    r.attach(bus, EventType::AbyssPitFall);
    r.attach(bus, EventType::MaddeningWhispers);
    r.attach(bus, EventType::DholeTremors);
    r.attach(bus, EventType::DholeAmbush);
    r.attach(bus, EventType::DungeonEntered);
}

// Find a room index carrying a hazard of the given type (-1 if none).
static int roomWithHazard(const ValeOfPnath& v, HazardType t) {
    for (const auto& h : v.hazards())
        if (h.type == t) return h.roomIndex;
    return -1;
}

static void testDeterminism() {
    EventBus b1, b2;
    ValeOfPnath a(b1, 1, 4242, Vec3{0, 0, 0});
    ValeOfPnath b(b2, 1, 4242, Vec3{0, 0, 0});
    CHECK(a.rooms().size() == b.rooms().size());
    CHECK(a.maxDepth() == b.maxDepth());
    CHECK(a.hazards().size() == b.hazards().size());
    for (size_t i = 0; i < a.rooms().size(); ++i) {
        CHECK(a.roomDepth(i) == b.roomDepth(i));
        CHECK(a.rooms()[i].x == b.rooms()[i].x);
        CHECK(a.rooms()[i].y == b.rooms()[i].y);
    }
    for (size_t i = 0; i < a.hazards().size(); ++i) {
        CHECK(a.hazards()[i].type == b.hazards()[i].type);
        CHECK(a.hazards()[i].roomIndex == b.hazards()[i].roomIndex);
    }
    // Different seed -> (almost surely) different layout.
    EventBus b3;
    ValeOfPnath c(b3, 1, 777, Vec3{0, 0, 0});
    bool sameRooms = a.rooms().size() == c.rooms().size();
    if (sameRooms) {
        for (size_t i = 0; i < a.rooms().size(); ++i)
            if (a.rooms()[i].x != c.rooms()[i].x ||
                a.rooms()[i].y != c.rooms()[i].y) {
                sameRooms = false;
                break;
            }
    }
    CHECK(!sameRooms);
}

static void testDepthsAndDread() {
    EventBus bus;
    ValeOfPnath v(bus, 1, 4242, Vec3{0, 0, 0});
    CHECK(v.rooms().size() > 12); // bigger than base dungeons (max 12)
    CHECK(v.roomDepth(0) == 0);
    CHECK(v.maxDepth() > 0);
    int deepest = v.deepestRoomIndex();
    CHECK(deepest > 0);
    CHECK(deepest < (int)v.rooms().size());
    CHECK(std::fabs(v.dreadAt(0) - 0.0f) < 1e-5f);
    CHECK(std::fabs(v.dreadAt(deepest) - 1.0f) < 1e-5f);
    // Depths are non-decreasing along the corridor chain.
    for (size_t i = 1; i < v.rooms().size(); ++i)
        CHECK(v.roomDepth(i) >= v.roomDepth(i - 1));
    // Dread is monotonic with depth.
    for (size_t i = 1; i < v.rooms().size(); ++i)
        CHECK(v.dreadAt((int)i) >= v.dreadAt((int)i - 1) - 1e-5f);
}

static void testAbyssPit() {
    EventBus bus;
    Recorder r;
    // Hunt a seed with an abyss pit.
    uint64_t seed = 4242;
    int pitRoom = -1;
    for (uint64_t s = 1; s < 40 && pitRoom < 0; ++s) {
        EventBus tmp;
        ValeOfPnath v(tmp, 1, s, Vec3{0, 0, 0});
        pitRoom = roomWithHazard(v, HazardType::AbyssPit);
        if (pitRoom >= 0) seed = s;
    }
    CHECK(pitRoom >= 0);
    ValeOfPnath v(bus, 1, seed, Vec3{0, 0, 0});
    attachVale(r, bus);
    pitRoom = roomWithHazard(v, HazardType::AbyssPit);
    v.registerSurfaceEntity(7);
    CHECK(v.enter(7));
    CHECK(v.traverseTo(7, pitRoom));
    CHECK(r.count(EventType::AbyssPitFall) == 1);
    const GameEvent* e = r.first(EventType::AbyssPitFall);
    CHECK(e != nullptr);
    int depth = v.roomDepth((size_t)pitRoom);
    float expected = ValeOfPnath::ABYSS_PIT_BASE_DAMAGE +
                     depth * ValeOfPnath::ABYSS_PIT_PER_DEPTH;
    CHECK(std::fabs(e->amount - expected) < 1e-3f);
    CHECK(e->sourceId == 7); // the falling entity
    // Once per entity: traversing again does not re-fire.
    CHECK(v.traverseTo(7, 0));
    CHECK(v.traverseTo(7, pitRoom));
    CHECK(r.count(EventType::AbyssPitFall) == 1);
}

static void testWhispers() {
    EventBus bus;
    Recorder r;
    uint64_t seed = 4242;
    int wRoom = -1;
    for (uint64_t s = 1; s < 40 && wRoom < 0; ++s) {
        EventBus tmp;
        ValeOfPnath v(tmp, 1, s, Vec3{0, 0, 0});
        wRoom = roomWithHazard(v, HazardType::Whispers);
        if (wRoom >= 0) seed = s;
    }
    CHECK(wRoom >= 0);
    ValeOfPnath v(bus, 1, seed, Vec3{0, 0, 0});
    attachVale(r, bus);
    wRoom = roomWithHazard(v, HazardType::Whispers);
    v.registerSurfaceEntity(9);
    CHECK(v.enter(9));
    CHECK(v.traverseTo(9, wRoom));
    CHECK(r.count(EventType::MaddeningWhispers) == 1);
    const GameEvent* e = r.first(EventType::MaddeningWhispers);
    CHECK(e != nullptr);
    float dread = v.dreadAt(wRoom);
    float expected = ValeOfPnath::WHISPER_BASE_FEAR +
                     dread * ValeOfPnath::WHISPER_DREAD_FEAR;
    CHECK(std::fabs(e->amount - expected) < 1e-3f);
    CHECK(e->targetId == 9); // the maddened entity
    // Whispers fire every traversal (dread grinds you down).
    CHECK(v.traverseTo(9, 0));
    CHECK(v.traverseTo(9, wRoom));
    CHECK(r.count(EventType::MaddeningWhispers) == 2);
}

static void testDholeTunnel() {
    EventBus bus;
    Recorder r;
    uint64_t seed = 4242;
    int tRoom = -1;
    for (uint64_t s = 1; s < 60 && tRoom < 0; ++s) {
        EventBus tmp;
        ValeOfPnath v(tmp, 1, s, Vec3{0, 0, 0});
        tRoom = roomWithHazard(v, HazardType::DholeTunnel);
        if (tRoom >= 0) seed = s;
    }
    CHECK(tRoom >= 0);
    ValeOfPnath v(bus, 1, seed, Vec3{0, 0, 0});
    attachVale(r, bus);
    tRoom = roomWithHazard(v, HazardType::DholeTunnel);
    v.setDholeEntityId(1234);
    v.registerSurfaceEntity(11);
    CHECK(v.enter(11));
    // First traversal: tremors (telegraph), no ambush yet.
    CHECK(v.traverseTo(11, tRoom));
    CHECK(r.count(EventType::DholeTremors) == 1);
    CHECK(r.count(EventType::DholeAmbush) == 0);
    // Second traversal: the ambush strikes.
    CHECK(v.traverseTo(11, 0));
    CHECK(v.traverseTo(11, tRoom));
    CHECK(r.count(EventType::DholeAmbush) == 1);
    const GameEvent* e = r.first(EventType::DholeAmbush);
    CHECK(e != nullptr);
    CHECK(e->sourceId == 1234); // attributed to the dhole
    CHECK(e->targetId == 11);
    float dread = v.dreadAt(tRoom);
    float expected = ValeOfPnath::DHOLE_AMBUSH_BASE_DAMAGE +
                     dread * ValeOfPnath::DHOLE_AMBUSH_DREAD_DAMAGE;
    CHECK(std::fabs(e->amount - expected) < 1e-3f);
    // Tunnel re-primes: a third traversal telegraphs again.
    CHECK(v.traverseTo(11, 0));
    CHECK(v.traverseTo(11, tRoom));
    CHECK(r.count(EventType::DholeTremors) == 2);
    CHECK(r.count(EventType::DholeAmbush) == 1);
}

static void testDholeEntity() {
    Dhole d(1, Vec3{10, 0, 5});
    CHECK(d.species() == "dhole");
    CHECK(d.maxHp() == Dhole::DHOLE_MAX_HP);
    CHECK(d.feral());
    CHECK(d.burrowed()); // starts burrowed
    d.surface();
    CHECK(!d.burrowed());
    d.burrow();
    CHECK(d.burrowed());
    CHECK(d.fearAuraRadius() > 0.0f);
    CHECK(d.fearAuraStrength() > 0.0f);
    // Dhole is a Monstrosity (feral, hostile to everything).
    CHECK(d.type() == EntityType::Monstrosity);
}

static void testDholeModelSlot() {
    // No CC0 dhole model exists: the slot is an intentional "" placeholder
    // and the engine falls back to the procedural serpent/worm shape.
    CHECK(ModelCatalog::creatureModel("dhole").empty());
}

static void testRelicVault() {
    EventBus bus;
    ValeOfPnath v(bus, 1, 4242, Vec3{100, 0, 100});
    int deepest = v.deepestRoomIndex();
    CHECK(deepest > 0);
    Vec3 relic = v.valeRelicSpot();
    Vec3 guard = v.guardianSpawnPos();
    // Vault positions are real world positions, not the entrance fallback.
    CHECK(relic.x != 100.0f || relic.z != 100.0f);
    CHECK(guard.x != 100.0f || guard.z != 100.0f);
    // Guardian spawns at the deepest room's center.
    const DungeonRoom& r = v.rooms()[static_cast<size_t>(deepest)];
    Vec3 expect = v.cellToWorld(r.centerX(), r.centerY());
    CHECK(std::fabs(guard.x - expect.x) < 1e-3f);
    CHECK(std::fabs(guard.z - expect.z) < 1e-3f);
}

static void testValeBiggerThanBase() {
    EventBus b1, b2;
    DungeonInstance base(b1, 1, 4242, Vec3{0, 0, 0});
    ValeOfPnath vale(b2, 2, 4242, Vec3{0, 0, 0});
    CHECK(vale.width() > base.width());
    CHECK(vale.rooms().size() >= base.rooms().size());
}

static void testLegalNames() {
    // No forbidden Mythos terms in any wave-13 user-facing string.
    const char* forbidden[] = {"Tindalos", "Derleth", "Call of Cthulhu"};
    std::string haystack;
    haystack += hazardTypeName(HazardType::AbyssPit);
    haystack += hazardTypeName(HazardType::Whispers);
    haystack += hazardTypeName(HazardType::DholeTunnel);
    Dhole d(1, Vec3{0, 0, 0});
    haystack += d.species();
    for (const char* f : forbidden) CHECK(haystack.find(f) == std::string::npos);
    // "dhole" itself is on the legal allowlist (Lovecraft-original).
    CHECK(d.species() == "dhole");
}

int main() {
    testDeterminism();
    testDepthsAndDread();
    testAbyssPit();
    testWhispers();
    testDholeTunnel();
    testDholeEntity();
    testDholeModelSlot();
    testRelicVault();
    testValeBiggerThanBase();
    testLegalNames();
    std::cout << "wave13: " << checks << " checks, " << failures
              << " failures\n";
    return failures == 0 ? 0 : 1;
}
