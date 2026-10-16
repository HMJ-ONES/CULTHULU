// CULT-ULHU micro-dev regression tests: Domination point-capture callout.
//
// Seizing the point used to arrive silently — the score ticked up but the
// console showed nothing and the exported GitHub match log recorded no
// capture. Now the driver announces "*** team X seizes the point! ***"
// ("*** YOUR TEAM seizes the point! ***" for the joined human's team) and
// MatchLogger records a CAPTURE line. These pin:
//
// 1. CapturePointMode publishes PointCaptured with faction = new holder
//    exactly once per holder change (flip t0 -> t1 -> more ticks).
// 2. A contested/empty point clears the holder but publishes nothing new
//    until a team holds uncontested again.
// 3. MatchLogger records "CAPTURE t1 seized the point" on the production
//    Match bus (exported story), alongside the older KILL lines.
// 4. Initial match start with nobody on the point publishes nothing.

#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "modes/CapturePointMode.h"
#include "modes/Match.h"
#include "modes/MatchLogger.h"

#include <iostream>
#include <string>
#include <vector>

using namespace cultulhu;

static int failures = 0;
#define CHECK(cond)                                        \
    do {                                                   \
        if (!(cond)) {                                     \
            std::cout << "FAIL line " << __LINE__ << ": "  \
                      << #cond << "\n";                     \
            ++failures;                                    \
        }                                                  \
    } while (0)

struct Hit {
    int faction;
};

int main() {
    // ---------- mode level: publish contract --------------------------
    {
        EventBus bus;
        GameClock clock;
        std::vector<Hit> hits;
        bus.subscribe(EventType::PointCaptured,
                      [&hits](const GameEvent& e) {
                          hits.push_back({e.faction});
                      });

        CapturePointMode mode(bus, clock);
        mode.setupDominationPoint();

        // Empty point at start: nothing published.
        for (int i = 0; i < 10; ++i) mode.update(1.0);
        CHECK(hits.empty());

        // 1. Team 0 takes it uncontested: exactly one event, faction=0.
        mode.setOccupants(0, 1, 0);
        mode.update(1.0);
        CHECK(hits.size() == 1);
        CHECK(hits[0].faction == 0);
        // Holding longer publishes nothing more.
        for (int i = 0; i < 30; ++i) mode.update(1.0);
        CHECK(hits.size() == 1);

        // 2. Contested: holder clears (-1), but no capture event for "none".
        mode.setOccupants(0, 1, 1);
        mode.update(1.0);
        CHECK(hits.size() == 1);
        // Team 1 takes it: second event, faction=1.
        mode.setOccupants(0, 0, 1);
        mode.update(1.0);
        CHECK(hits.size() == 2);
        CHECK(hits[1].faction == 1);
        for (int i = 0; i < 30; ++i) mode.update(1.0);
        CHECK(hits.size() == 2);
    }

    // ---------- production path: Match + MatchLogger ------------------
    {
        EventBus bus;
        GameClock clock;
        RNG rng(17);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        MatchLogger logger;
        logger.attach(m);

        GameEvent e(EventType::PointCaptured);
        e.faction = 1;
        bus.publish(e);

        int captures = 0;
        for (const auto& entry : logger.events()) {
            if (entry.kind == "CAPTURE") {
                ++captures;
                CHECK(entry.text == "CAPTURE t1 seized the point");
            }
        }
        CHECK(captures == 1);
    }

    if (failures == 0) std::cout << "ALL POINTCAPTURE CHECKS PASSED\n";
    return failures ? 1 : 0;
}
