// CULT-ULHU micro-dev regression tests: match-start ("whistle") callout.
//
// MatchStarted has fired since wave 16 (GameMode::noteMatchTick, first
// update tick, tag = "capture"/"moba") but nothing announced it: the
// driver printed a plain status line and the exported GitHub match log
// jumped straight to KILL lines. Now the driver console gets an arena
// callout and the log gets an opening START beat. These pin, through the
// production start/tick path (botfill + attach ordering mirrors the
// driver's `match domination` flow):
//
// 1. Domination: first update tick publishes exactly one MatchStarted,
//    tag "capture"; a second tick does not re-fire.
// 2. The logger records one START line: "START Domination (5v5)".
// 3. DOTGOO: tag "moba", START line "START DOTGOO (5v5)".
// 4. A fresh match in the same session re-fires (no latch confusion), and
//    its log carries the new START line.

#include "modes/Match.h"
#include "modes/MatchLogger.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

#include <cstdint>
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
    // ---------- 1+2. Domination: exactly-once + START log beat ----------
    {
        EventBus bus; GameClock clock; RNG rng(6101);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        MatchLogger logger;
        logger.attach(m); // driver order: attach before the first tick
        std::vector<std::string> tags;
        bus.subscribe(EventType::MatchStarted, [&](const GameEvent& e) {
            tags.push_back(e.tag);
        });
        m.update(1.0);
        CHECK(tags.size() == 1);
        CHECK(tags[0] == "capture");
        for (int i = 0; i < 10; ++i) m.update(1.0);
        CHECK(tags.size() == 1); // the whistle blows once
        int starts = 0;
        for (const auto& e : logger.events()) {
            if (e.kind == "START") {
                ++starts;
                CHECK(e.text == "START Domination (5v5)");
            }
        }
        CHECK(starts == 1);
    }

    // ---------- 3. DOTGOO: tag + START log beat ----------
    {
        EventBus bus; GameClock clock; RNG rng(6102);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        MatchLogger logger;
        logger.attach(m);
        std::vector<std::string> tags;
        bus.subscribe(EventType::MatchStarted, [&](const GameEvent& e) {
            tags.push_back(e.tag);
        });
        m.update(1.0);
        CHECK(tags.size() == 1);
        CHECK(tags[0] == "moba");
        int starts = 0;
        for (const auto& e : logger.events()) {
            if (e.kind == "START") {
                ++starts;
                CHECK(e.text == "START DOTGOO (5v5)");
            }
        }
        CHECK(starts == 1);
    }

    // ---------- 4. A fresh match re-fires; its log gets its own START ----------
    {
        EventBus bus; GameClock clock; RNG rng(6103);
        std::vector<std::string> tags;
        bus.subscribe(EventType::MatchStarted, [&](const GameEvent& e) {
            tags.push_back(e.tag);
        });
        MatchLogger logger;
        {
            Match m1(bus, clock, rng);
            CHECK(m1.start("capture"));
            logger.attach(m1);
            m1.update(1.0);
        }
        {
            Match m2(bus, clock, rng);
            CHECK(m2.start("capture"));
            logger.attach(m2); // detach + clear rides the 8 PM fix
            m2.update(1.0);
            int starts = 0;
            for (const auto& e : logger.events()) {
                if (e.kind == "START") ++starts;
            }
            CHECK(starts == 1); // the old START was cleared, new one landed
        }
        CHECK(tags.size() == 2); // each match blows its own whistle
    }

    if (failures == 0) std::cout << "ALL MATCHSTART OK\n";
    return failures ? 1 : 0;
}
