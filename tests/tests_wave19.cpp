// CULT-ULHU wave 19 tests: art-budget enforcement + perf smoke.
//
// This is the automated budget guard: it scans EVERY decoded .glb under
// assets/ and fails the build if any model or texture exceeds its tier cap,
// or if the total decoded payload (glb + png, excluding .b64 sidecars)
// exceeds the global cap. Future art additions cannot silently bloat the
// game — the host's PC simulates the world for everyone.
//
// Tier caps (measured 2026-10-08; headroom is deliberate, bloat is not):
//   assets/world/                      <=  2000 tris/model (scenery props)
//   assets/characters/, assets/creatures/ <= 8000 tris/model (KayKit rigs
//                                          peak at ~7k today)
//   every .png                         <= 1024 px max dimension
//   total decoded payload              <= 12 MB (was ~5.0 MB on 2026-10-08)
//
// Perf smoke: map load + Vale generation are timed with generous ceilings
// (these catch pathological regressions, not micro-jitter; real numbers go
// in docs/perf_notes.md).

#include "assets/GlbInfo.h"
#include "core/EventBus.h"
#include "core/Vec3.h"
#include "world/MapLoader.h"
#include "world/ValeOfPnath.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace cultulhu;
namespace fs = std::filesystem;

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

static const uint64_t kMaxTrisWorld = 2000;
static const uint64_t kMaxTrisActor = 8000;
static const uint64_t kMaxTexDim = 1024;
static const uint64_t kMaxTotalBytes = 12ull * 1024ull * 1024ull;

static bool underWorld(const std::string& p) {
    return p.find("assets/world/") != std::string::npos;
}

// --- minimal PNG dimension reader (IHDR only, no decode) ---
static bool readPngDims(const std::string& path, uint32_t& w, uint32_t& h) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    unsigned char sig[8];
    f.read(reinterpret_cast<char*>(sig), 8);
    const unsigned char want[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    for (int i = 0; i < 8; ++i) if (sig[i] != want[i]) return false;
    unsigned char ihdr[8];
    f.read(reinterpret_cast<char*>(ihdr), 8);  // length + "IHDR"
    if (ihdr[4] != 'I' || ihdr[5] != 'H' || ihdr[6] != 'D' || ihdr[7] != 'R')
        return false;
    unsigned char dims[8];
    f.read(reinterpret_cast<char*>(dims), 8);
    w = (uint32_t(dims[0]) << 24) | (uint32_t(dims[1]) << 16) |
        (uint32_t(dims[2]) << 8) | uint32_t(dims[3]);
    h = (uint32_t(dims[4]) << 24) | (uint32_t(dims[5]) << 16) |
        (uint32_t(dims[6]) << 8) | uint32_t(dims[7]);
    return w > 0 && h > 0;
}

static void testModelBudgets() {
    const std::string prefix = repoPrefix();
    uint64_t totalBytes = 0;
    int worldModels = 0, actorModels = 0, otherModels = 0;
    uint64_t maxWorldTris = 0, maxActorTris = 0;
    for (auto it = fs::recursive_directory_iterator(prefix + "assets");
         it != fs::recursive_directory_iterator(); ++it) {
        const std::string p = it->path().string();
        const std::string ext = it->path().extension().string();
        if (ext == ".b64") continue;  // sidecars don't ship decoded
        if (ext == ".glb") {
            GlbStats st = readGlbStats(p);
            CHECK(st.ok);
            if (!st.ok) {
                std::cout << "  unreadable GLB: " << p << " (" << st.error
                          << ")\n";
                continue;
            }
            const uint64_t cap =
                underWorld(p) ? kMaxTrisWorld : kMaxTrisActor;
            CHECK(st.triangles <= cap);
            if (st.triangles > cap)
                std::cout << "  OVER BUDGET: " << p << " (" << st.triangles
                          << " tris, cap " << cap << ")\n";
            if (underWorld(p)) { ++worldModels; maxWorldTris = std::max(maxWorldTris, st.triangles); }
            else { ++actorModels; maxActorTris = std::max(maxActorTris, st.triangles); }
            totalBytes += fs::file_size(it->path());
        } else if (ext == ".png") {
            uint32_t w = 0, h = 0;
            CHECK(readPngDims(p, w, h));
            if (w && h) {
                CHECK(std::max(w, h) <= kMaxTexDim);
                if (std::max(w, h) > kMaxTexDim)
                    std::cout << "  TEXTURE OVER BUDGET: " << p << " (" << w
                              << "x" << h << ")\n";
            }
            totalBytes += fs::file_size(it->path());
        }
    }
    std::cout << "  world models: " << worldModels << " (max " << maxWorldTris
              << " tris), actor models: " << actorModels << " (max "
              << maxActorTris << " tris), decoded bytes: " << totalBytes
              << "\n";
    CHECK(worldModels > 0);
    CHECK(actorModels > 0);
    CHECK(totalBytes <= kMaxTotalBytes);
    if (totalBytes > kMaxTotalBytes)
        std::cout << "  PAYLOAD OVER BUDGET: " << totalBytes << " bytes (cap "
                  << kMaxTotalBytes << ")\n";
    (void)otherModels;
}

static void testPerfSmoke() {
    const std::string prefix = repoPrefix();
    using clock = std::chrono::steady_clock;
    // Map load (both shipped maps).
    for (const char* m : {"assets/maps/ruined_city.map",
                          "assets/maps/eldritch_battlefield.map"}) {
        auto t0 = clock::now();
        MapData map = MapLoader::load(prefix + m);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      clock::now() - t0)
                      .count();
        std::cout << "  load " << m << ": " << map.placements.size()
                  << " placements in " << ms << " ms\n";
        CHECK(!map.placements.empty());
        CHECK(ms < 30000);  // pathological-regression ceiling only
    }
    // Vale generation (seeded).
    {
        auto t0 = clock::now();
        EventBus bus;
        ValeOfPnath vale(bus, 1, 4242, Vec3{0, 0, 0});
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      clock::now() - t0)
                      .count();
        std::cout << "  ValeOfPnath(4242): " << vale.rooms().size()
                  << " rooms in " << ms << " ms\n";
        CHECK(!vale.rooms().empty());
        CHECK(ms < 30000);
    }
}

int main() {
    testModelBudgets();
    testPerfSmoke();
    std::cout << "wave19 checks: " << checks << ", failures: " << failures
              << "\n";
    return failures == 0 ? 0 : 1;
}
