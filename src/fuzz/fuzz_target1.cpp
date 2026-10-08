// Wave 9d FUZZ TARGET 1 — event bus.
//
// Publishes random/malformed GameEvents into a fully-wired driver game
// (every real subscriber: BeliefSystem, ExertionSystem, CultManager,
// DirectiveExecutor, DreamSystem, LunaticSystem, ambient/ritual AI, ...):
//   - random type IDs: valid, just-past-Count, and negative (invalid)
//   - garbage payloads: NaN / +-Inf / denormal / huge amounts, extreme or
//     NaN positions, empty/huge/odd tags, wild ids and factions
// Must not crash, throw, or hang. Deterministic: `seed` drives everything.

#include "fuzz/fuzz_driver.h"
#include "fuzz/fuzz_targets.h"
#include "fuzz/fuzz_util.h"

#include "core/Events.h"
#include "core/Vec3.h"

#include <cmath>
#include <cstdio>
#include <limits>

using namespace cultulhu;

namespace fuzz {
namespace {

GameEvent randomEvent(FuzzRng& rng) {
    GameEvent e;
    const double r = rng.uni01();
    int t;
    if (r < 0.88) {
        t = static_cast<int>(rng.below(static_cast<size_t>(EventType::Count)));
    } else if (r < 0.94) {
        // Just past the end (invalid).
        t = static_cast<int>(EventType::Count) + static_cast<int>(rng.below(8));
    } else {
        // Negative (invalid).
        t = -1 - static_cast<int>(rng.below(8));
    }
    e.type = static_cast<EventType>(t);
    e.sourceId = rng.chance(0.15) ? 0 : rng.u64();
    e.targetId = rng.chance(0.15) ? 0 : rng.u64();
    e.faction = rng.chance(0.70) ? static_cast<int>(rng.range(-2, 5))
                                 : static_cast<int>(rng.range(-100000, 100000));
    e.amount = randomFloat(rng);
    e.pos = Vec3(randomFloat(rng), randomFloat(rng), randomFloat(rng));
    e.tag = randomTag(rng);
    return e;
}

std::string describeEvent(const GameEvent& e) {
    char buf[256];
    const float a = e.amount;
    std::snprintf(buf, sizeof(buf),
                  "type=%d src=%llu tgt=%llu faction=%d amount=%g%s pos=(%g,"
                  "%g,%g) taglen=%zu tag=",
                  static_cast<int>(e.type),
                  (unsigned long long)e.sourceId,
                  (unsigned long long)e.targetId, e.faction,
                  std::isnan(a) ? 0.0 : (double)a,
                  std::isnan(a) ? "[NaN]"
                                : (std::isinf(a) ? (a > 0 ? "[+Inf]" : "[-Inf]")
                                                 : ""),
                  (double)e.pos.x, (double)e.pos.y, (double)e.pos.z,
                  e.tag.size());
    return std::string(buf) + escapeForReport(e.tag);
}

} // namespace

int runTarget1(uint64_t seed, size_t iterations) {
    logf("[fuzz] target1 event-bus: seed=%llu iterations=%zu\n",
                 (unsigned long long)seed, iterations);
    FuzzRng rng(seed);
    BetaGame* g = newGame();
    EventBus& bus = gameBus(*g);
    int rc = 0;

    for (size_t i = 0; i < iterations; ++i) {
        const GameEvent e = randomEvent(rng);
        Watchdog wd;
        wd.start();
        bool threw = false;
        std::string what;
        try {
            bus.publish(e);
        } catch (const std::exception& ex) {
            threw = true;
            what = std::string("std::exception: ") + ex.what();
        } catch (...) {
            threw = true;
            what = "unknown exception";
        }
        const double ms = wd.elapsedMs();
        if (threw) {
            reportFailure("event-bus", seed, i, describeEvent(e), what);
            rc = 1;
            break;
        }
        if (ms > Watchdog::kHangThresholdMs) {
            char w[96];
            std::snprintf(w, sizeof(w),
                          "hang: single publish took %.1fms (> %.0fms)",
                          ms, Watchdog::kHangThresholdMs);
            reportFailure("event-bus", seed, i, describeEvent(e), w);
            rc = 1;
            break;
        }
    }

    deleteGame(g);
    logf("[fuzz] target1 event-bus: %s\n",
                 rc == 0 ? "clean" : "BUG FOUND");
    return rc;
}

} // namespace fuzz
