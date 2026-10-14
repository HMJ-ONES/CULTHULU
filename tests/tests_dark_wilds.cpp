// CULT-ULHU dark wilds tests: the generated surface world (Oct 14, 2026).
// Covers: dark_wilds.map parses; 6 zones in file order with atmosphere;
// every placement references an existing .glb under the tri budget;
// placement heights sane; `spawn` lines parse into MapData::spawns with
// valid species/zone/count; loader rejects bad spawn lines loudly;
// generator outputs (terrain OBJ, heightmap, detail texture) exist;
// the three new procgen props resolve via ModelCatalog.

#include "assets/GlbInfo.h"
#include "assets/ModelCatalog.h"
#include "world/MapLoader.h"
#include "world/WorldMap.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

// ctest runs from build/; the driver/tests also run from the repo root.
static std::string repoPrefix() {
    std::ifstream p("assets/maps/dark_wilds.map");
    if (p) return "";
    return "../";
}

static MapData loadWilds() {
    return MapLoader::load(repoPrefix() + "assets/maps/dark_wilds.map");
}

// ---------------- map parses: zones + atmosphere ----------------

static void testZonesAndAtmosphere() {
    MapData map = loadWilds();
    ++checks;
    CHECK(map.name == "dark_wilds");
    CHECK(map.zones.size() == 6);
    const char* want[6] = {"deep_cavern", "cavern_approach", "razed_city",
                           "dead_swamp", "drywood", "highlands"};
    for (size_t i = 0; i < 6 && i < map.zones.size(); ++i)
        CHECK(map.zones[i].name == want[i]);
    // specific zones beat general ones in file order
    CHECK(map.zones[0].ambient.ambientFear == 85);
    // deep cavern: lightless bowl, stars drowned
    const ZoneDef& cav = map.zones[0];
    CHECK(cav.atmosphere.stars == 0.0f);
    CHECK(cav.atmosphere.fogDensity > 0.8f);
    // swamp murk is green-tinted
    const ZoneDef& swamp = map.zones[3];
    CHECK(swamp.atmosphere.fogG > swamp.atmosphere.fogR);
    CHECK(swamp.atmosphere.fogDensity > 0.7f);
    std::cout << "  zones: " << map.zones.size() << "\n";
}

// ---------------- placements: files exist, budgets, heights ----------------

static void testPlacements() {
    MapData map = loadWilds();
    ++checks;
    CHECK(map.placements.size() >= 300);
    std::set<std::string> distinct;
    for (const auto& p : map.placements) distinct.insert(p.modelPath);
    uint64_t totalBytes = 0;
    for (const auto& m : distinct) {
        const std::string f = repoPrefix() + m;
        GlbStats st = readGlbStats(f);
        CHECK(st.ok);
        if (!st.ok) {
            std::cout << "  unreadable GLB: " << f << "\n";
            continue;
        }
        CHECK(st.triangles < 5000);
        std::ifstream in(f, std::ios::binary | std::ios::ate);
        CHECK(static_cast<bool>(in));
        totalBytes += static_cast<uint64_t>(in.tellg());
    }
    CHECK(totalBytes < 30ull * 1024ull * 1024ull);
    // heights baked from the height field: cavern floor deep, hills high
    bool sawDeep = false, sawHigh = false;
    for (const auto& p : map.placements) {
        CHECK(p.pos.y > -35.0f && p.pos.y < 45.0f);
        if (p.zone == "deep_cavern" && p.pos.y < -20.0f) sawDeep = true;
        if (p.zone == "highlands" && p.pos.y > 12.0f) sawHigh = true;
        bool known = false;
        for (const auto& z : map.zones)
            if (z.name == p.zone) { known = true; break; }
        CHECK(known);
        CHECK(p.scale > 0.0f);
    }
    CHECK(sawDeep);
    CHECK(sawHigh);
    // razed city really has three clumped complexes: wall segments cluster
    int wardWalls = 0;
    for (const auto& p : map.placements) {
        if (p.zone == "razed_city" &&
            p.modelPath == "assets/world/buildings/stone-wall-damaged.glb")
            ++wardWalls;
    }
    CHECK(wardWalls >= 10);
    std::cout << "  placements: " << map.placements.size()
              << ", distinct models: " << distinct.size()
              << ", total bytes: " << totalBytes << "\n";
}

// ---------------- spawns ----------------

static void testSpawns() {
    MapData map = loadWilds();
    ++checks;
    CHECK(!map.spawns.empty());
    int beasts = 0;
    const auto& creatures = ModelCatalog::creatureModels();
    for (const auto& s : map.spawns) {
        CHECK(creatures.count(s.species) == 1);
        if (!creatures.count(s.species))
            std::cout << "  unknown species: " << s.species << "\n";
        CHECK(s.count > 0);
        CHECK(s.pos.y > -35.0f && s.pos.y < 45.0f);
        bool known = false;
        for (const auto& z : map.zones)
            if (z.name == s.zone) { known = true; break; }
        CHECK(known);
        beasts += s.count;
    }
    CHECK(beasts >= 25);  // "lots of feral beasts"
    // the swamp is the deadliest ground
    int swampBeasts = 0;
    for (const auto& s : map.spawns)
        if (s.zone == "dead_swamp") swampBeasts += s.count;
    CHECK(swampBeasts >= 20);
    std::cout << "  spawn entries: " << map.spawns.size()
              << ", beasts: " << beasts << "\n";
}

static void testSpawnErrors() {
    // bad arity
    try {
        MapLoader::load(repoPrefix() +
                        "assets/maps/dark_wilds.map");
        ++checks;  // the real file must parse
    } catch (const std::exception& e) {
        std::cout << "FAIL: real map did not parse: " << e.what() << "\n";
        ++failures; ++checks;
    }
    // craft bad files in /tmp
    auto bad = [](const std::string& body) {
        const std::string p = "/tmp/cultulhu_spawn_bad.map";
        std::ofstream f(p);
        f << "map t\nzone z -10 -10 -10 10 10 10 10\n" << body;
        f.close();
        try {
            MapLoader::load(p);
            return false;  // should have thrown
        } catch (const MapParseError&) {
            return true;
        }
    };
    CHECK(bad("spawn ghoul 1 2 3\n"));                    // arity
    CHECK(bad("spawn ghoul 1 2 3 0 z\n"));                // count <= 0
    CHECK(bad("spawn ghoul 1 2 3 2 nowhere\n"));          // unknown zone
    CHECK(bad("spawn ghoul a 2 3 2 z\n"));                // non-numeric
    std::remove("/tmp/cultulhu_spawn_bad.map");
}

// ---------------- generator outputs exist ----------------

static void testGeneratorOutputs() {
    const std::string pre = repoPrefix();
    auto exists = [&](const std::string& rel) {
        std::ifstream f(pre + rel, std::ios::binary);
        CHECK(static_cast<bool>(f));
        if (!f) std::cout << "  missing: " << rel << "\n";
    };
    exists("assets/world/terrain/dark_wilds_ground.obj");
    exists("assets/world/terrain/dark_wilds_height.png");
    exists("assets/world/textures/ground-detail-dark.png");
    // terrain OBJ is a real mesh, not a stub
    std::ifstream obj(pre + "assets/world/terrain/dark_wilds_ground.obj");
    int verts = 0, faces = 0;
    std::string line;
    while (std::getline(obj, line)) {
        if (line.rfind("v ", 0) == 0) ++verts;
        if (line.rfind("f ", 0) == 0) ++faces;
    }
    CHECK(verts == 256 * 256);
    CHECK(faces == 255 * 255 * 2);
    std::cout << "  terrain: " << verts << " verts, " << faces << " tris\n";
}

// ---------------- new props resolve ----------------

static void testNewProps() {
    const std::string pre = repoPrefix();
    for (const char* key : {"reed_cluster", "rock_arch", "cave_mouth"}) {
        const std::string path = ModelCatalog::lookup(key);
        CHECK(!path.empty());
        std::ifstream f(pre + path, std::ios::binary);
        CHECK(static_cast<bool>(f));
        if (f) {
            GlbStats st = readGlbStats(pre + path);
            CHECK(st.ok && st.triangles < 2000 && st.triangles > 50);
        }
    }
}

int main() {
    testZonesAndAtmosphere();
    testPlacements();
    testSpawns();
    testSpawnErrors();
    testGeneratorOutputs();
    testNewProps();

    std::cout << "dark_wilds checks: " << checks << ", failures: " << failures
              << "\n";
    return failures == 0 ? 0 : 1;
}
