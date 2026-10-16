// CULT-ULHU wave 7a tests: world, dungeons, altars, buildings, construction.
// Covers: Zone/WorldMap lookup, seeded dungeon determinism (rooms and
// caves), entrance enter/exit, boss-death completion, altar tiers/ritual
// math/escort bookkeeping, building types, construction-site progress
// scaling, and BuilderAI belief-weighted order generation.

#include "ai/BuilderAI.h"
#include "beliefs/BeliefSystem.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "entities/Structures.h"
#include "entities/Units.h"
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

// ---------------- Zone / WorldMap ----------------

static ZoneDef makeZoneDef(const std::string& name, float x0, float z0,
                           float x1, float z1) {
    ZoneDef d;
    d.name = name;
    d.min = Vec3(x0, 0.0f, z0);
    d.max = Vec3(x1, 10.0f, z1);
    d.ambient.ambientFear = 25.0f;
    d.ambient.relicSpawnChance = 0.3f;
    d.relicSpots.push_back(Vec3((x0 + x1) * 0.5f, 0.0f, (z0 + z1) * 0.5f));
    SpawnPoint sp{Vec3(x0 + 1.0f, 0.0f, z0 + 1.0f), EntityType::Civilian, 5};
    d.spawns.push_back(sp);
    return d;
}

static void testZoneWorldMap() {
    WorldMap map("testland");
    map.addZone(makeZoneDef("plains", 0.0f, 0.0f, 100.0f, 100.0f));
    map.addZone(makeZoneDef("hills", 200.0f, 200.0f, 300.0f, 300.0f));

    CHECK(map.zoneCount() == 2);
    const Zone* z = map.zoneAt(Vec3(50.0f, 5.0f, 50.0f));
    CHECK(z != nullptr);
    CHECK(z->name() == "plains");
    CHECK(z->contains(Vec3(0.0f, 0.0f, 0.0f)));   // inclusive bounds
    CHECK(z->contains(Vec3(100.0f, 10.0f, 100.0f)));
    CHECK(!z->contains(Vec3(101.0f, 5.0f, 50.0f)));
    CHECK(z->ambient().ambientFear == 25.0f);
    CHECK(z->spawnPoints().size() == 1);
    CHECK(z->spawnPoints()[0].entityType == EntityType::Civilian);
    CHECK(z->spawnPoints()[0].count == 5);
    CHECK(z->relicSpots().size() == 1);

    const Zone* z2 = map.zoneAt(Vec3(250.0f, 1.0f, 250.0f));
    CHECK(z2 != nullptr && z2->name() == "hills");
    CHECK(map.zoneAt(Vec3(150.0f, 1.0f, 150.0f)) == nullptr); // between zones

    // Overlap: first zone in insertion order wins.
    map.addZone(makeZoneDef("overlap", 40.0f, 40.0f, 60.0f, 60.0f));
    // NOTE: addZone may reallocate; re-fetch pointers afterwards.
    z = map.zoneAt(Vec3(50.0f, 5.0f, 50.0f));
    CHECK(z != nullptr && z->name() == "plains");

    // randomPoint stays inside bounds; empty relic list -> origin.
    RNG rng(7);
    Vec3 p = z->randomPoint(rng);
    CHECK(z->contains(p));
    ZoneDef empty;
    empty.name = "void";
    Zone ze(empty);
    CHECK(ze.randomRelicSpot(rng).x == 0.0f);
    CHECK(ze.randomRelicSpot(rng).z == 0.0f);
}

// ---------------- Dungeons ----------------

static bool hasTile(const DungeonInstance& d, char want) {
    for (char c : d.tiles())
        if (c == want) return true;
    return false;
}

static void testDungeonDeterminism() {
    EventBus bus;
    DungeonInstance a(bus, 1, 12345, Vec3(0, 0, 0));
    DungeonInstance b(bus, 1, 12345, Vec3(0, 0, 0));
    DungeonInstance c(bus, 1, 99999, Vec3(0, 0, 0));

    CHECK(a.tiles() == b.tiles()); // same seed -> same layout
    CHECK(a.tiles() != c.tiles()); // different seed -> different layout
    CHECK(!a.rooms().empty());
    CHECK(hasTile(a, 'E')); // entrance marked
    CHECK(hasTile(a, '.')); // carved floor exists
    // Entrance cell is walkable.
    bool entranceFloor = false;
    for (int y = 0; y < a.height() && !entranceFloor; ++y)
        for (int x = 0; x < a.width(); ++x)
            if (a.tile(x, y) == 'E' && a.isFloor(x, y)) {
                entranceFloor = true;
                break;
            }
    CHECK(entranceFloor);
    CHECK(!a.relicSpots().empty()); // rooms past the entrance hold relics
    CHECK(!a.isFloor(-1, 0));       // out of bounds is not floor
}

static void testCaveInstance() {
    EventBus bus;
    CaveInstance a(bus, 2, 4242, Vec3(10, 0, 10));
    CaveInstance b(bus, 2, 4242, Vec3(10, 0, 10));
    CHECK(a.tiles() == b.tiles()); // deterministic too
    CHECK(hasTile(a, 'E'));
    CHECK(hasTile(a, '.'));
    CHECK(!a.rooms().empty()); // stamped chambers tracked as rooms
}

static void testDungeonEnterExit() {
    EventBus bus;
    std::vector<GameEvent> seen;
    bus.subscribe(EventType::DungeonEntered,
                  [&](const GameEvent& e) { seen.push_back(e); });
    bus.subscribe(EventType::DungeonExited,
                  [&](const GameEvent& e) { seen.push_back(e); });

    DungeonInstance d(bus, 7, 1, Vec3(5, 0, 5));
    d.setEntranceEntity(1001);
    CHECK(d.entranceEntity() == 1001);

    Cultist surf(FACTION_CTHULHU, Vec3(5, 0, 5));
    Cultist other(FACTION_CTHULHU, Vec3(6, 0, 5));
    d.registerSurfaceEntity(surf.id());
    d.registerSurfaceEntity(other.id());

    CHECK(d.enter(surf.id()));
    CHECK(d.isInside(surf.id()));
    CHECK(d.insideCount() == 1);
    CHECK(!d.enter(surf.id())); // already inside
    CHECK(!d.enter(31337));     // never registered

    CHECK(seen.size() == 1);
    CHECK(seen[0].type == EventType::DungeonEntered);
    CHECK(seen[0].sourceId == surf.id());
    CHECK(seen[0].targetId == 7);

    CHECK(d.exit(surf.id()));
    CHECK(!d.isInside(surf.id()));
    CHECK(!d.exit(surf.id())); // already outside
    CHECK(seen.size() == 2);
    CHECK(seen[1].type == EventType::DungeonExited);
}

static void testDungeonBossCompletion() {
    EventBus bus;
    std::vector<GameEvent> done;
    bus.subscribe(EventType::DungeonCompleted,
                  [&](const GameEvent& e) { done.push_back(e); });

    DungeonInstance d(bus, 9, 3, Vec3());
    Cultist boss(FACTION_NEUTRAL, Vec3());
    Cultist minion(FACTION_NEUTRAL, Vec3());
    d.registerSurfaceEntity(boss.id());
    d.registerSurfaceEntity(minion.id());
    d.enter(boss.id());
    d.enter(minion.id());
    d.setBossId(boss.id());

    d.onEntityDied(minion.id()); // non-boss death: nothing
    CHECK(!d.completed());
    CHECK(done.empty());
    CHECK(!d.isInside(minion.id())); // the fallen leaves the roster

    d.onEntityDied(boss.id()); // boss falls: dungeon complete
    CHECK(d.completed());
    CHECK(done.size() == 1);
    CHECK(done[0].sourceId == boss.id());
    CHECK(done[0].targetId == 9);

    d.onEntityDied(boss.id()); // idempotent
    CHECK(done.size() == 1);
}

// ---------------- Altars & buildings ----------------

static void testAltar() {
    // Altar is the last EntityType enumerator.
    CHECK(static_cast<int>(EntityType::Altar) ==
          static_cast<int>(EntityType::Artifact) + 1);

    Altar a(FACTION_CTHULHU, Vec3(1, 0, 1));
    CHECK(a.type() == EntityType::Altar);
    CHECK(a.buildingType() == BuildingType::Altar);
    CHECK(a.tier() == Altar::MIN_TIER);
    CHECK_CLOSE(a.ritualPowerMult(), 1.0f, 1e-5f);

    CHECK(a.upgrade() == 2);
    CHECK_CLOSE(a.ritualPowerMult(), 1.5f, 1e-5f);
    CHECK(a.upgrade() == 3);
    CHECK_CLOSE(a.ritualPowerMult(), 2.0f, 1e-5f);
    CHECK(a.upgrade() == 3); // clamped at MAX_TIER
    CHECK(a.tier() == Altar::MAX_TIER);

    // Escort bookkeeping.
    a.assignCaptive(501);
    a.assignCaptive(502);
    CHECK(a.assignedCaptives().size() == 2);
    a.unassignCaptive(501);
    CHECK(a.assignedCaptives().size() == 1);
    CHECK(a.assignedCaptives()[0] == 502);
    a.unassignCaptive(999); // unknown id: no-op
    CHECK(a.assignedCaptives().size() == 1);
    a.clearCaptives();
    CHECK(a.assignedCaptives().empty());
}

static void testBuildingTypes() {
    // Legacy ctor keeps working: still a Building, default type Wall.
    Building legacy(FACTION_CTHULHU, Vec3());
    CHECK(legacy.type() == EntityType::Building);
    CHECK(legacy.buildingType() == BuildingType::Wall);

    // Typed ctor picks per-type default HP.
    Building wall(FACTION_CTHULHU, Vec3(), BuildingType::Wall);
    CHECK(wall.buildingType() == BuildingType::Wall);
    CHECK_CLOSE(wall.maxHp(), 2500.0f, 1e-3f);
    Building tower(FACTION_CTHULHU, Vec3(), BuildingType::Watchtower);
    CHECK_CLOSE(tower.maxHp(), 600.0f, 1e-3f);
    // Explicit HP overrides the default.
    Building custom(FACTION_CTHULHU, Vec3(), BuildingType::Trap, 123.0f);
    CHECK_CLOSE(custom.maxHp(), 123.0f, 1e-3f);
    CHECK(custom.buildingType() == BuildingType::Trap);

    CHECK(std::string(buildingTypeName(BuildingType::Portal)) == "Portal");
}

static void testConstructionSite() {
    // Progress scales linearly with builder count.
    ConstructionSite solo(FACTION_CTHULHU, Vec3(), BuildingType::Barracks);
    ConstructionSite trio(FACTION_CTHULHU, Vec3(), BuildingType::Barracks);
    solo.addBuilder(11);
    trio.addBuilder(21);
    trio.addBuilder(22);
    trio.addBuilder(23);
    CHECK(solo.builderCount() == 1);
    CHECK(trio.builderCount() == 3);
    CHECK(trio.hasBuilder(22));
    trio.removeBuilder(22);
    CHECK(trio.builderCount() == 2);
    trio.addBuilder(22);

    solo.update(30.0);
    trio.update(30.0);
    CHECK_CLOSE(trio.progress(), 3.0f * solo.progress(), 1e-5f);
    CHECK_CLOSE(solo.progress(), 0.25f, 1e-5f); // 30s of 120s with 1 builder

    // No builders -> stalled.
    ConstructionSite idle(FACTION_CTHULHU, Vec3(), BuildingType::Wall);
    idle.update(1000.0);
    CHECK(idle.progress() == 0.0f);
    CHECK(!idle.finished());

    // Completion hands back the finished building.
    solo.update(90.0);
    CHECK(solo.finished());
    auto built = solo.complete();
    CHECK(built != nullptr);
    CHECK(built->buildingType() == BuildingType::Barracks);
    CHECK(built->type() == EntityType::Building);
    CHECK(solo.complete() == nullptr); // one-shot

    // Altar target completes into a real Altar.
    ConstructionSite shrine(FACTION_CTHULHU, Vec3(), BuildingType::Altar);
    shrine.addBuilder(31);
    shrine.update(120.0);
    auto altarBuilt = shrine.complete();
    CHECK(altarBuilt != nullptr);
    Altar* asAltar = dynamic_cast<Altar*>(altarBuilt.get());
    CHECK(asAltar != nullptr);
    CHECK(asAltar->type() == EntityType::Altar);
    CHECK(asAltar->tier() == 1);

    // Repair site: complete() yields nothing; the AI heals the target.
    ConstructionSite repair(FACTION_CTHULHU, Vec3(), BuildingType::Wall, 777);
    CHECK(repair.isRepair());
    CHECK(repair.repairTargetId() == 777);
    repair.addBuilder(41);
    repair.update(120.0);
    CHECK(repair.complete() == nullptr);
}

// ---------------- BuilderAI ----------------

struct AIHarness {
    EventBus bus;
    GameClock clock;
    RNG rng{0xBEEF};
    BeliefSystem beliefs;
    PowerSystem power;
    CultManager cult;
    ExertionSystem exertion;
    BuilderAI ai;

    AIHarness()
        : beliefs(bus, clock),
          cult(bus, clock, rng),
          exertion(bus, beliefs, power, cult, rng),
          ai(bus, rng, beliefs, exertion, cult) {}
};

static void testBuilderWeighting() {
    AIHarness h;
    // Rig: Magic belief active and burning hot.
    h.beliefs.restoreActive({Belief::Magic});
    h.exertion.addExertion(Belief::Magic, 90.0f);
    CHECK(h.exertion.exertion(Belief::Magic) >= 50.0f);

    int counts[6] = {};
    for (int i = 0; i < 300; ++i) {
        BuildingType t = h.ai.pickBuildingType();
        counts[static_cast<int>(t)]++;
    }
    const int altar = counts[static_cast<int>(BuildingType::Altar)];
    for (int i = 0; i < 6; ++i) {
        if (i == static_cast<int>(BuildingType::Altar)) continue;
        CHECK(altar > counts[i]); // Magic-high cult favors altars
    }

    // Rig War instead: walls and barracks dominate.
    AIHarness h2;
    h2.beliefs.restoreActive({Belief::War});
    h2.exertion.addExertion(Belief::War, 90.0f);
    int wcounts[6] = {};
    for (int i = 0; i < 300; ++i)
        wcounts[static_cast<int>(h2.ai.pickBuildingType())]++;
    CHECK(wcounts[static_cast<int>(BuildingType::Wall)] >
          wcounts[static_cast<int>(BuildingType::Altar)]);
    CHECK(wcounts[static_cast<int>(BuildingType::Barracks)] >
          wcounts[static_cast<int>(BuildingType::Portal)]);
}

static void testBuilderAIIntegration() {
    AIHarness h;
    std::vector<GameEvent> started, progressed, completed;
    h.bus.subscribe(EventType::BuildStarted,
                    [&](const GameEvent& e) { started.push_back(e); });
    h.bus.subscribe(EventType::BuildProgress,
                    [&](const GameEvent& e) { progressed.push_back(e); });
    h.bus.subscribe(EventType::BuildCompleted,
                    [&](const GameEvent& e) { completed.push_back(e); });

    // One loyal hard worker, one shirker (devotion 10 < threshold 40).
    Cultist& loyal = h.cult.recruit();
    loyal.setDevotion(90.0f);
    Cultist& shirker = h.cult.recruit();
    shirker.setDevotion(10.0f);
    std::vector<Cultist*> cultists{&loyal, &shirker};

    std::vector<std::unique_ptr<ConstructionSite>> sites;
    std::vector<Building*> buildings;
    Settlement s{Vec3(0, 0, 0), 60.0f, {}};
    // Pre-seed one order so the first decision pass is deterministic (new
    // orders otherwise appear on a 50% chance per pass).
    BuildOrder seed;
    seed.type = BuildingType::Barracks;
    seed.pos = Vec3(10, 0, 0);
    s.queue.push_back(seed);
    std::vector<Settlement> settlements{s};

    // First update jumps past the decision interval -> a site breaks ground.
    h.ai.update(30.0, cultists, sites, settlements, buildings);
    CHECK(!started.empty());
    CHECK(sites.size() == 1);
    CHECK(sites[0]->hasBuilder(loyal.id()));   // loyal cultist got assigned
    CHECK(!sites[0]->hasBuilder(shirker.id())); // shirker stays idle
    CHECK(started[0].sourceId == sites[0]->id());
    CHECK(!started[0].tag.empty());

    // Fast-forward until the build completes (1 builder -> 120s).
    for (int i = 0; i < 100 && completed.empty(); ++i)
        h.ai.update(2.0, cultists, sites, settlements, buildings);
    CHECK(!completed.empty());
    CHECK(completed[0].sourceId == started[0].sourceId);
    CHECK(completed[0].tag == started[0].tag);
    // BuildProgress was throttled: at most ~11 events per site.
    CHECK(progressed.size() <= 11 * started.size());

    auto finished = h.ai.takeFinishedBuildings();
    CHECK(!finished.empty());
    CHECK(std::string(buildingTypeName(finished[0]->buildingType())) ==
          started[0].tag);
    CHECK(h.ai.takeFinishedBuildings().empty()); // drained

    // Repair path: damage a building, crank Reconstruction, watch it heal.
    Building hall(FACTION_CTHULHU, Vec3(10, 0, 0), BuildingType::Barracks);
    hall.takeDamage(hall.maxHp() * 0.5f);
    CHECK(hall.state() == BuildingState::Damaged);
    buildings.push_back(&hall);
    h.beliefs.restoreActive({Belief::Reconstruction});
    h.exertion.addExertion(Belief::Reconstruction, 90.0f);

    const float hpBefore = hall.hp();
    for (int i = 0; i < 60 && hall.hp() <= hpBefore + 1.0f; ++i)
        h.ai.update(5.0, cultists, sites, settlements, buildings);
    CHECK(hall.hp() > hpBefore + 1.0f); // repair crew healed it
}

static void testBuilderOrdersAppear() {
    // Fresh cult with plenty of idle labor: over many decision passes new
    // build orders must appear on their own (50% per pass while idle labor
    // exists and the settlement isn't saturated).
    AIHarness h;
    std::vector<GameEvent> started;
    h.bus.subscribe(EventType::BuildStarted,
                    [&](const GameEvent& e) { started.push_back(e); });

    std::vector<Cultist*> cultists;
    for (int i = 0; i < 5; ++i) {
        Cultist& c = h.cult.recruit();
        c.setDevotion(80.0f);
        cultists.push_back(&c);
    }
    std::vector<std::unique_ptr<ConstructionSite>> sites;
    std::vector<Building*> buildings;
    std::vector<Settlement> settlements{Settlement{Vec3(0, 0, 0), 60.0f, {}}};

    for (int i = 0; i < 30; ++i)
        h.ai.update(25.0, cultists, sites, settlements, buildings);
    // P(no order in 30 passes) = 2^-30; any seed passes this.
    CHECK(started.size() >= 2);
}

int main() {
    testZoneWorldMap();
    testDungeonDeterminism();
    testCaveInstance();
    testDungeonEnterExit();
    testDungeonBossCompletion();
    testAltar();
    testBuildingTypes();
    testConstructionSite();
    testBuilderWeighting();
    testBuilderAIIntegration();
    testBuilderOrdersAppear();

    std::cout << "wave7a checks: " << checks << ", failures: " << failures
              << "\n";
    return failures == 0 ? 0 : 1;
}
