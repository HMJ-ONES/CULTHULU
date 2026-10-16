// CULT-ULHU micro-dev regression tests: Domination "overtime" announcement.
//
// A tied Domination match at the 600s whistle used to flip into sudden
// death silently — match status showed "OVERTIME" but the console printed
// nothing and the exported match log trailed off with no record of the
// flip. The transition now publishes EventType::OvertimeStarted exactly
// once, which gets both a console callout (main.cpp) and an OVERTIME line
// in the GitHub match log. These pin:
//
// 1. Tie at the time limit: exactly one OvertimeStarted; the mode reports
//    overtime() with zero overtime elapsed on the transition tick (the
//    fresh-tick guard from tests_wave21 still holds).
// 2. Non-tie at the time limit: no event, no overtime, match is over.
// 3. No repeat: later ticks never re-fire the event; first score in
//    overtime ends the match.
// 4. Production path: a real Match driven to the tied whistle logs exactly
//    one OVERTIME line via MatchLogger.

#include "modes/Match.h"
#include "modes/MatchLogger.h"
#include "modes/CapturePointMode.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

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

int main() {
    // ---------- mode-level staging (mirrors tests_micro_matchpoint) ----
    {
        EventBus bus;
        GameClock clock;
        int hits = 0;
        bus.subscribe(EventType::OvertimeStarted,
                      [&hits](const GameEvent&) { ++hits; });

        // 2. Non-tie first: team 0 leads 5-0 at the whistle.
        {
            CapturePointMode mode(bus, clock);
            mode.setupDominationPoint();
            mode.notePlayerKill(0);
            mode.update(CapturePointMode::TIME_LIMIT);
            CHECK(hits == 0);
            CHECK(!mode.overtime());
            CHECK(mode.isOver());
            CHECK(mode.winner() == 0);
        }

        // 1. 5-5 tie at the whistle: one event, fresh-tick guard holds.
        CapturePointMode mode(bus, clock);
        mode.setupDominationPoint();
        mode.notePlayerKill(0);
        mode.notePlayerKill(1);
        mode.update(CapturePointMode::TIME_LIMIT); // single huge dt: transition tick
        CHECK(hits == 1);
        CHECK(mode.overtime());
        CHECK(mode.overtimeElapsed() == 0.0);
        CHECK(!mode.isOver()); // tied in overtime: the match continues

        // 3. More ticks: no second event.
        for (int i = 0; i < 30; ++i) mode.update(1.0);
        CHECK(hits == 1);

        // 3b. First score in overtime ends the match.
        mode.notePlayerKill(0);
        mode.update(1.0);
        CHECK(hits == 1);
        CHECK(mode.isOver());
        CHECK(mode.winner() == 0);
    }

    // ---------- production path: real Match + MatchLogger --------------
    {
        EventBus bus; GameClock clock; RNG rng(17);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        MatchLogger logger;
        logger.attach(m);
        // No players at all: 0-0 is a tie, and nobody can score.
        m.update(CapturePointMode::TIME_LIMIT);
        CHECK(m.capture() && m.capture()->overtime());
        int ot = 0;
        for (const auto& e : logger.events()) {
            if (e.kind == "OVERTIME") {
                ++ot;
                CHECK(e.text.find("first score wins") != std::string::npos);
            }
        }
        CHECK(ot == 1);
    }

    if (failures == 0) std::cout << "ALL OVERTIME CHECKS PASSED\n";
    return failures ? 1 : 0;
}
