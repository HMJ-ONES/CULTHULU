// CULT-ULHU micro-dev regression tests: "Great Old One slain" arena callout.
//
// DOTGOO matches ended with the god's death narratively silent: MobaDefense
// published GreatOldOneSlain (sourceId = killer, faction = losing team, pos =
// base position) but nothing on the console announced it — the log and the
// screen jumped from GOOATTACK warnings straight to the VICTORY line, with no
// record of the god falling or who struck the killing blow.
//
// Now the driver announces it ("*** YOUR GREAT OLD ONE HAS FALLEN! ***" for
// the defending player's team, "*** Team X's Great Old One falls to <killer>!
// ***" otherwise; "the minions" when sourceId is 0) and MatchLogger records a
// GOOSLAIN line carrying the team and the killer attribution, so the exported
// story has an ending. FX already maps GreatOldOneSlain to the raze preset
// (5 PM beauty sweep, pinned in tests_wave36). These pin:
//
// 1. Killing blow: exactly one GreatOldOneSlain with sourceId = killer id,
//    faction = defending team, pos = base position.
// 2. Post-mortem damage stays silent (gooAlive guard — no double slain).
// 3. MatchLogger: a GreatOldOneSlain on the bus lands a GOOSLAIN line
//    carrying the team and the killer ("minions" when sourceId is 0).
//
// The console subscriber wording itself lives in main.cpp and is verified
// live in the driver (micro-dev convention), like the other arena callouts.

#include "modes/MobaDefense.h"
#include "modes/Match.h"
#include "modes/MatchLogger.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "core/Events.h"

#include <iostream>
#include <string>

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

// damageBase is protected; the tests need the production path, not a copy.
class TestMoba : public MobaDefense {
  public:
    using MobaDefense::MobaDefense;
    using MobaDefense::damageBase;
};

int main() {
    // ---------- 1-2: production loop contract via the real damageBase ----------
    {
        EventBus bus; GameClock clock; RNG rng(79);
        TestMoba moba(bus, clock, rng);
        // No lanes built: laneOpenFor() is true, so the GOO takes damage.
        moba.setBase(0, Vec3(-150, 0, 0), 1500.0f);
        moba.setBase(1, Vec3(150, 0, 0), 1500.0f);

        int slain = 0;
        GameEvent last;
        bus.subscribe(EventType::GreatOldOneSlain,
                      [&](const GameEvent& e) { ++slain; last = e; });

        // 1. Killing blow: exactly one event with the full payload.
        moba.damageBase(1, 5000.0f, 4242);
        CHECK(slain == 1);
        CHECK(last.sourceId == 4242);
        CHECK(last.faction == 1);
        CHECK(last.pos.x == 150.0f && last.pos.y == 0.0f && last.pos.z == 0.0f);

        // 2. Hitting the corpse: nothing at all — no double slain.
        moba.damageBase(1, 5000.0f, 4242);
        CHECK(slain == 1);

        // Team 0's GOO was never touched: no slain for it.
        CHECK(last.faction == 1);
    }

    // ---------- 3: MatchLogger line ----------
    {
        EventBus bus; GameClock clock; RNG rng(80);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        MatchLogger logger;
        logger.attach(m);

        GameEvent e(EventType::GreatOldOneSlain);
        e.faction = 1;
        e.sourceId = 0; // minions did it — no player to attribute
        bus.publish(e);

        int lines = 0;
        for (const auto& entry : logger.events()) {
            if (entry.kind == "GOOSLAIN") {
                ++lines;
                CHECK(entry.text.find("t1") != std::string::npos);
                CHECK(entry.text.find("falls") != std::string::npos);
                CHECK(entry.text.find("minions") != std::string::npos);
            }
        }
        CHECK(lines == 1);
    }

    if (failures == 0) std::cout << "ALL GOOSLAIN CHECKS PASSED\n";
    return failures ? 1 : 0;
}
