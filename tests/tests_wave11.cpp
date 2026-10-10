// CULT-ULHU wave 11 tests (PART B): art-pass map layout, loader, wiring.
// Covers: ruined_city.map parses cleanly; every placement references an
// existing .glb; no two placements share coordinates; per-model triangle
// counts < 5000 (via the GlbInfo header reader); total asset size < 30MB;
// zone defs feed WorldMap; MapLoader fails loudly on bad input;
// ModelCatalog/modelPathFor wiring resolves to real files.
//
// Triangle counts are read from the GLB headers with src/assets/GlbInfo.h
// (a minimal C++ reader: JSON chunk -> accessors -> primitives). This was
// chosen over MANIFEST.md so the budget check works on the actual files
// even if the manifest is stale; the counts were cross-checked against
// MANIFEST.md during reconciliation (all match).

#include "assets/GlbInfo.h"
#include "assets/ModelCatalog.h"
#include "entities/Structures.h"
#include "world/MapLoader.h"
#include "world/WorldMap.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <tuple>
#include <vector>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

// ctest runs from build/; the driver/tests also run from the repo root.
static std::string repoPrefix() {
    std::ifstream p("assets/maps/ruined_city.map");
    if (p) return "";
    return "../";
}

static const uint64_t kMaxTrisPerModel = 5000;
static const uint64_t kMaxTotalBytes = 30ull * 1024ull * 1024ull;

// ---------------- map parses ----------------

static void testMapParses() {
    const std::string path = repoPrefix() + "assets/maps/ruined_city.map";
    MapData map;
    try {
        map = MapLoader::load(path);
    } catch (const std::exception& e) {
        std::cout << "FAIL: map did not parse: " << e.what() << "\n";
        ++failures; ++checks;
        return;
    }
    ++checks;
    CHECK(map.name == "ruined_city");
    CHECK(map.zones.size() == 4);
    CHECK(map.placements.size() == 90);
    // Zone defs feed WorldMap unchanged (insertion order = file order).
    WorldMap wm("surface");
    map.applyZones(wm);
    CHECK(wm.zoneCount() == 4);
    const Zone* z = wm.zoneAt(Vec3{0, 0, 300});  // altar plaza
    CHECK(z != nullptr && z->name() == "cult_base");
    z = wm.zoneAt(Vec3{-280, 0, 20});            // graveyard
    CHECK(z != nullptr && z->name() == "graveyard");
    z = wm.zoneAt(Vec3{0, 0, 0});                // old city streets
    CHECK(z != nullptr && z->name() == "old_city");
    // Ambient fear sanity from the file.
    for (const auto& zd : map.zones) {
        CHECK(zd.ambient.ambientFear >= 0.0f &&
              zd.ambient.ambientFear <= 100.0f);
        CHECK(!zd.name.empty());
    }
    // Every placement names a declared zone.
    std::set<std::string> zoneNames;
    for (const auto& zd : map.zones) zoneNames.insert(zd.name);
    for (const auto& p : map.placements) {
        CHECK(zoneNames.count(p.zone) == 1);
        CHECK(p.scale > 0.0f);
        CHECK(!p.modelPath.empty());
    }
}

// ---------------- loader fails loudly ----------------

static void writeTmp(const std::string& path, const std::string& body) {
    std::ofstream o(path, std::ios::trunc);
    o << body;
}

static void testLoaderErrors() {
    const std::string tmp = "/tmp/wave11_badmap.map";
    // Unknown directive.
    writeTmp(tmp, "map x\nfrobnicate 1 2 3\n");
    try { MapLoader::load(tmp); CHECK(false); }
    catch (const MapParseError& e) { CHECK(e.line() == 2); ++checks; }

    // Bad arity.
    writeTmp(tmp, "map x\nzone only_name\n");
    try { MapLoader::load(tmp); CHECK(false); }
    catch (const MapParseError& e) { CHECK(e.line() == 2); ++checks; }

    // Unknown zone reference.
    writeTmp(tmp, "map x\nzone z 0 0 0 1 1 1 10\n"
                  "place assets/world/cult/banner.glb 0 0 0 0 1 nowhere\n");
    try { MapLoader::load(tmp); CHECK(false); }
    catch (const MapParseError& e) { CHECK(e.line() == 3); ++checks; }

    // Non-numeric coordinate.
    writeTmp(tmp, "map x\nzone z 0 0 0 1 1 1 10\n"
                  "place assets/world/cult/banner.glb oops 0 0 0 1 z\n");
    try { MapLoader::load(tmp); CHECK(false); }
    catch (const MapParseError& e) { CHECK(e.line() == 3); ++checks; }

    // Missing file.
    try { MapLoader::load("/tmp/wave11_does_not_exist.map"); CHECK(false); }
    catch (const MapParseError&) { ++checks; }

    std::remove(tmp.c_str());
}

// ---------------- file existence + unique coords ----------------

static void testFilesExistAndUniqueCoords() {
    const std::string prefix = repoPrefix();
    MapData map = MapLoader::load(prefix + "assets/maps/ruined_city.map");
    std::set<std::string> distinct;
    for (const auto& p : map.placements) distinct.insert(p.modelPath);
    for (const auto& m : distinct) {
        std::ifstream f(prefix + m, std::ios::binary);
        CHECK(static_cast<bool>(f));
        if (!f)
            std::cout << "  MISSING: " << prefix + m << "\n";
    }
    // No two placements share identical coordinates.
    std::set<std::tuple<float, float, float>> coords;
    for (const auto& p : map.placements)
        coords.insert({p.pos.x, p.pos.y, p.pos.z});
    CHECK(coords.size() == map.placements.size());
}

// ---------------- triangle + size budgets ----------------

static void testArtBudgets() {
    const std::string prefix = repoPrefix();
    MapData map = MapLoader::load(prefix + "assets/maps/ruined_city.map");
    std::set<std::string> distinct;
    for (const auto& p : map.placements) distinct.insert(p.modelPath);
    uint64_t totalBytes = 0;
    for (const auto& m : distinct) {
        const std::string f = prefix + m;
        GlbStats st = readGlbStats(f);
        CHECK(st.ok);
        if (!st.ok) {
            std::cout << "  unreadable GLB: " << f << " (" << st.error
                      << ")\n";
            continue;
        }
        CHECK(st.triangles < kMaxTrisPerModel);
        if (st.triangles >= kMaxTrisPerModel)
            std::cout << "  over budget: " << f << " (" << st.triangles
                      << " tris)\n";
        std::ifstream in(f, std::ios::binary | std::ios::ate);
        CHECK(static_cast<bool>(in));
        totalBytes += static_cast<uint64_t>(in.tellg());
    }
    CHECK(totalBytes < kMaxTotalBytes);
    std::cout << "  distinct models: " << distinct.size()
              << ", total bytes: " << totalBytes << "\n";
}

// ---------------- wiring ----------------

static void testWiring() {
    const std::string prefix = repoPrefix();
    // Every catalog entry resolves to a real file.
    for (const auto& [logical, path] : ModelCatalog::worldModels()) {
        CHECK(!path.empty());
        std::ifstream f(prefix + path, std::ios::binary);
        CHECK(static_cast<bool>(f));
        if (!f) std::cout << "  catalog MISSING: " << logical << "\n";
    }
    CHECK(ModelCatalog::lookup("nope").empty());
    CHECK(ModelCatalog::categoryOf(
              "assets/world/cult/altar-stone.glb") == "cult");
    CHECK(ModelCatalog::categoryOf("nope.glb").empty());
    // Every BuildingType maps to a real model file.
    for (int i = 0; i < static_cast<int>(BuildingType::Portal) + 1; ++i) {
        BuildingType t = static_cast<BuildingType>(i);
        const std::string p = modelPathFor(t);
        CHECK(!p.empty());
        std::ifstream f(prefix + p, std::ios::binary);
        CHECK(static_cast<bool>(f));
    }
    // Altar entities reference the altar model.
    Altar altar(1, Vec3{0, 0, 300});
    CHECK(altar.modelPath() ==
          ModelCatalog::lookup("altar"));
    CHECK(altar.modelPath() == "assets/world/cult/altar-stone.glb");
    Building wall(1, Vec3{0, 0, 0}, BuildingType::Wall);
    CHECK(wall.modelPath() == ModelCatalog::lookup("ruined_wall"));
}

int main() {
    testMapParses();
    testLoaderErrors();
    testFilesExistAndUniqueCoords();
    testArtBudgets();
    testWiring();

    std::cout << "wave11 checks: " << checks << ", failures: " << failures
              << "\n";
    return failures == 0 ? 0 : 1;
}
