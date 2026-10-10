// CULT-ULHU wave 38 tests: LOD distance tuning + culling hints in map files.
//
// Guards the wave-38 work:
//   - both .map files parse with the new `lod` directive; every placed
//     model is covered by exactly one lod rule (no duplicates, no gaps);
//   - rule sanity: hideAt in [10, 2000], lowAt == 0 or < hideAt;
//   - per-placement hints parse: cull=<m>, nevercull; both together,
//     cull<=0, and unknown hints are parse errors;
//   - lod validation errors: missing hide, hide<=0, low>=hide, low<0,
//     duplicate rule, lod before map declaration, unknown lod key;
//   - MapData::cullDistanceFor resolution order: neverCull -> 0,
//     explicit cullDist -> cullDist, model's lod hideAt, else 0.

#include "world/MapLoader.h"

#include <cmath>
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
    std::ifstream p("assets/maps/ruined_city.map");
    if (p) return "";
    return "../";
}

static MapData loadMap(const char* name) {
    return MapLoader::load(repoPrefix() + std::string("assets/maps/") + name);
}

// ---------------- both maps parse with full LOD coverage ----------------

static void testLodCoverage(const char* name) {
    MapData d = loadMap(name);
    CHECK(!d.lodRules.empty());
    // Every placed model has exactly one rule.
    std::set<std::string> placed, ruled;
    for (const auto& p : d.placements) placed.insert(p.modelPath);
    for (const auto& r : d.lodRules) {
        ruled.insert(r.modelPath);
        // Sanity bounds on the authored distances.
        CHECK(r.hideAt >= 10.0f && r.hideAt <= 2000.0f);
        CHECK(r.lowAt == 0.0f || (r.lowAt > 0.0f && r.lowAt < r.hideAt));
        // Rule targets a model actually placed in this map.
        CHECK(placed.count(r.modelPath) == 1);
    }
    CHECK(ruled == placed);
    std::cout << "  " << name << ": " << d.placements.size()
              << " placements, " << d.lodRules.size() << " lod rules\n";
}

// ---------------- cullDistanceFor resolution ----------------

static void testCullResolution() {
    MapData d = loadMap("ruined_city.map");

    // neverCull wins over everything (altar-stone plaza centerpiece).
    bool foundNever = false;
    for (const auto& p : d.placements) {
        if (p.neverCull) {
            foundNever = true;
            CHECK(d.cullDistanceFor(p) == 0.0f);
        }
    }
    CHECK(foundNever);

    // Explicit cullDist overrides the model's lod rule (gravestones cull=180,
    // rule for gravestone-cross is 250/120).
    bool foundOverride = false;
    float ruleHide = -1.0f;
    for (const auto& r : d.lodRules)
        if (r.modelPath.find("gravestone-cross") != std::string::npos)
            ruleHide = r.hideAt;
    for (const auto& p : d.placements) {
        if (p.cullDist > 0.0f && p.modelPath.find("gravestone-cross") !=
            std::string::npos) {
            foundOverride = true;
            CHECK(std::fabs(d.cullDistanceFor(p) - 180.0f) < 0.001f);
        }
    }
    CHECK(foundOverride);
    CHECK(ruleHide == 250.0f);

    // Plain placement resolves to its model's rule.
    bool foundPlain = false;
    for (const auto& p : d.placements) {
        if (p.cullDist < 0.0f && !p.neverCull &&
            p.modelPath.find("stone-wall-damaged") != std::string::npos) {
            foundPlain = true;
            CHECK(std::fabs(d.cullDistanceFor(p) - 250.0f) < 0.001f);
            break;
        }
    }
    CHECK(foundPlain);

    // Placement with no rule and no hints -> 0 (always visible).
    MapData empty;
    PlacedProp p;
    p.modelPath = "assets/world/props/ghost.glb";
    CHECK(empty.cullDistanceFor(p) == 0.0f);

    // neverCull beats an explicit cullDist in the struct too.
    PlacedProp both;
    both.cullDist = 100.0f;
    both.neverCull = true;
    CHECK(empty.cullDistanceFor(both) == 0.0f);
}

// ---------------- parser rejects bad lod ----------------

static void testLodErrors() {
    int caught = 0;
    const char* bad[] = {
        // lod before map declaration
        "lod assets/world/props/debris.glb hide=250\n",
        // missing hide
        "map m\nlod assets/world/props/debris.glb low=100\n",
        // hide <= 0
        "map m\nlod assets/world/props/debris.glb hide=0\n",
        // low >= hide
        "map m\nlod assets/world/props/debris.glb hide=100 low=100\n",
        // low > hide
        "map m\nlod assets/world/props/debris.glb hide=100 low=150\n",
        // negative low
        "map m\nlod assets/world/props/debris.glb hide=100 low=-5\n",
        // duplicate rule
        "map m\nlod assets/world/props/debris.glb hide=100\n"
        "lod assets/world/props/debris.glb hide=200\n",
        // unknown lod key
        "map m\nlod assets/world/props/debris.glb hide=100 far=9\n",
        // bare arg instead of key=value
        "map m\nlod assets/world/props/debris.glb 250\n",
        // unknown directive still rejected
        "map m\nzoom assets/world/props/debris.glb hide=100\n",
    };
    for (const char* body : bad) {
        const std::string path = repoPrefix() + "assets/maps/_wave38_bad.map";
        std::ofstream f(path);
        f << body;
        f.close();
        try {
            MapLoader::load(path);
        } catch (const MapParseError&) {
            ++caught;
        }
        std::remove(path.c_str());
    }
    CHECK(caught == 10);
}

// ---------------- parser rejects bad place hints ----------------

static void testPlaceHintErrors() {
    int caught = 0;
    const char* bad[] = {
        // unknown hint
        "map m\nzone z 0 0 0 10 10 10 10\n"
        "place assets/world/props/debris.glb 0 0 0 0 1 z glow=5\n",
        // cull <= 0
        "map m\nzone z 0 0 0 10 10 10 10\n"
        "place assets/world/props/debris.glb 0 0 0 0 1 z cull=0\n",
        // cull + nevercull conflict
        "map m\nzone z 0 0 0 10 10 10 10\n"
        "place assets/world/props/debris.glb 0 0 0 0 1 z cull=100 nevercull\n",
        // malformed cull value
        "map m\nzone z 0 0 0 10 10 10 10\n"
        "place assets/world/props/debris.glb 0 0 0 0 1 z cull=\n",
    };
    for (const char* body : bad) {
        const std::string path = repoPrefix() + "assets/maps/_wave38_bad2.map";
        std::ofstream f(path);
        f << body;
        f.close();
        try {
            MapLoader::load(path);
        } catch (const MapParseError&) {
            ++caught;
        }
        std::remove(path.c_str());
    }
    CHECK(caught == 4);
}

// ---------------- good place hints parse ----------------

static void testPlaceHintGood() {
    const std::string path = repoPrefix() + "assets/maps/_wave38_good.map";
    {
        std::ofstream f(path);
        f << "map m\nzone z 0 0 0 10 10 10 10\n"
             "place assets/world/props/debris.glb 0 0 0 0 1 z\n"
             "place assets/world/props/debris.glb 1 0 0 0 1 z cull=180\n"
             "place assets/world/props/debris.glb 2 0 0 0 1 z nevercull\n"
             "lod assets/world/props/debris.glb hide=250 low=120\n";
    }
    MapData d = MapLoader::load(path);
    std::remove(path.c_str());
    CHECK(d.placements.size() == 3);
    CHECK(d.placements[0].cullDist < 0.0f && !d.placements[0].neverCull);
    CHECK(std::fabs(d.placements[1].cullDist - 180.0f) < 0.001f);
    CHECK(!d.placements[1].neverCull);
    CHECK(d.placements[2].neverCull);
    CHECK(d.lodRules.size() == 1);
    CHECK(std::fabs(d.lodRules[0].hideAt - 250.0f) < 0.001f);
    CHECK(std::fabs(d.lodRules[0].lowAt - 120.0f) < 0.001f);
    CHECK(std::fabs(d.cullDistanceFor(d.placements[0]) - 250.0f) < 0.001f);
    CHECK(std::fabs(d.cullDistanceFor(d.placements[1]) - 180.0f) < 0.001f);
    CHECK(d.cullDistanceFor(d.placements[2]) == 0.0f);
}

// ---------------- LOD tuning report (informational) ----------------

static void testTuningReport() {
    // Report the farthest-reaching models per map: these dominate the
    // per-frame draw set on low-spec hosts.
    for (const char* name :
         {"ruined_city.map", "eldritch_battlefield.map"}) {
        MapData d = loadMap(name);
        float maxHide = 0.0f;
        size_t pinned = 0, overridden = 0;
        for (const auto& p : d.placements) {
            if (p.neverCull) { ++pinned; continue; }
            if (p.cullDist > 0.0f) ++overridden;
            maxHide = std::max(maxHide, d.cullDistanceFor(p));
        }
        std::cout << "  " << name << ": max cull " << maxHide << "m, "
                  << pinned << " pinned, " << overridden
                  << " placement overrides\n";
        CHECK(maxHide <= 900.0f);
    }
}

int main() {
    testLodCoverage("ruined_city.map");
    testLodCoverage("eldritch_battlefield.map");
    testCullResolution();
    testLodErrors();
    testPlaceHintErrors();
    testPlaceHintGood();
    testTuningReport();
    std::cout << "wave38: " << checks << " checks, " << failures
              << " failures\n";
    return failures == 0 ? 0 : 1;
}
