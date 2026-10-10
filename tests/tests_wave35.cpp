// CULT-ULHU wave 35 tests: forest / deep-ruins procgen props.
//
// Guards the wave-35 art pass:
//   - every placement in eldritch_battlefield.map names a decoded .glb
//     that exists on disk (catches typos before they reach the driver);
//   - no two placements share identical coordinates;
//   - the 8 new wave-35 procgen props exist and stay under the 2000-tri
//     world-prop budget (they also carry COLOR_0 vertex colors, which the
//     budget test counts as extra vertex data but not extra triangles).

#include "assets/GlbInfo.h"
#include "world/MapLoader.h"

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

static std::string repoPrefix() {
    std::ifstream p("assets/maps/eldritch_battlefield.map");
    if (p) return "";
    return "../";
}

// Every placement references a decoded .glb that exists on disk.
static void testBattlefieldPlacementFilesExist() {
    const std::string prefix = repoPrefix();
    MapData map = MapLoader::load(prefix +
                                  "assets/maps/eldritch_battlefield.map");
    CHECK(!map.placements.empty());
    std::set<std::string> distinct;
    for (const auto& pl : map.placements) distinct.insert(pl.modelPath);
    std::cout << "  " << map.placements.size() << " placements, "
              << distinct.size() << " distinct models\n";
    for (const auto& m : distinct) {
        std::ifstream f(prefix + m, std::ios::binary);
        if (!f) {
            std::cout << "  MISSING MODEL: " << m << "\n";
        }
        CHECK(static_cast<bool>(f));
    }
    // All 8 wave-35 props are actually used by the map.
    for (const char* prop :
         {"ash-thicket", "hanging-moss", "fungal-shelf", "root-tangle",
          "ruined-column-b", "fallen-lintel", "flagstone-slab",
          "broken-obelisk"}) {
        bool used = false;
        const std::string needle =
            std::string("assets/world/props/") + prop + ".glb";
        for (const auto& pl : map.placements)
            if (pl.modelPath == needle) { used = true; break; }
        if (!used) std::cout << "  UNUSED PROP: " << needle << "\n";
        CHECK(used);
    }
}

// No two placements share identical coordinates.
static void testBattlefieldUniqueCoords() {
    const std::string prefix = repoPrefix();
    MapData map = MapLoader::load(prefix +
                                  "assets/maps/eldritch_battlefield.map");
    std::set<std::tuple<float, float, float>> coords;
    for (const auto& pl : map.placements)
        coords.insert({pl.pos.x, pl.pos.y, pl.pos.z});
    if (coords.size() != map.placements.size())
        std::cout << "  DUPLICATE COORDS: " << map.placements.size()
                  << " placements, " << coords.size() << " unique\n";
    CHECK(coords.size() == map.placements.size());
}

// The 8 new procgen props exist and stay under the world-prop tri budget.
static void testWave35PropBudgets() {
    const std::string prefix = repoPrefix();
    for (const auto& [prop, maxTris] :
         std::vector<std::pair<const char*, uint64_t>>{
             {"ash-thicket", 2000}, {"hanging-moss", 2000},
             {"fungal-shelf", 2000}, {"root-tangle", 2000},
             {"ruined-column-b", 2000}, {"fallen-lintel", 2000},
             {"flagstone-slab", 2000}, {"broken-obelisk", 2000}}) {
        const std::string path = prefix + "assets/world/props/" + prop +
                                 ".glb";
        GlbStats st = readGlbStats(path);
        if (!st.ok) std::cout << "  GLB READ FAIL: " << path << " ("
                              << st.error << ")\n";
        CHECK(st.ok);
        if (st.ok && st.triangles > maxTris)
            std::cout << "  OVER BUDGET: " << path << " (" << st.triangles
                      << " tris, cap " << maxTris << ")\n";
        CHECK(st.ok && st.triangles <= maxTris);
    }
}

int main() {
    testBattlefieldPlacementFilesExist();
    testBattlefieldUniqueCoords();
    testWave35PropBudgets();
    std::cout << "wave35: " << checks << " checks, " << failures
              << " failures\n";
    return failures == 0 ? 0 : 1;
}
