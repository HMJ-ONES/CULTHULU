// CULT-ULHU micro-dev regression tests: arrival survey at `match join`.
//
// The driver's move survey (tests_micro_matchsurvey) fires only after a
// move, so a fresh join landed the player at their team base with no read
// of where the point or the enemies were. The driver's `match join`
// handler now prints Match::printMatchSurvey once, right at join. These
// pin the join-time contract in the exact post-join state the driver
// sees (no update ticks yet):
//
// 1. Domination: start, one bot on team 0, joinAsHuman(0), then the survey
//    — it must emit "the point:" with a distance + compass bearing (the
//    human spawns at their base, far from the point) and a "nearest enemy"
//    line. Never empty, never crashes with zero ticks run.
// 2. DOTGOO: start("moba"), bots on both teams, joinAsHuman — the survey
//    must have no point line but still name the nearest enemy with a
//    bearing.
// 3. Join with no enemies present: "no living enemies" (matches the move
//    survey's empty case, kept consistent at join time too).

#include "modes/Match.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

#include <iostream>
#include <sstream>
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

static std::string survey(Match& m, uint64_t humanId) {
    std::ostringstream ss;
    m.printMatchSurvey(ss, humanId);
    return ss.str();
}

int main() {
    // ---------- 1: Domination join-time survey ----------
    {
        EventBus bus; GameClock clock; RNG rng(7);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        m.addBot(0);   // slot for the human to take over
        m.botfill();   // the rest of both teams
        const uint64_t human = m.joinAsHuman(0);
        CHECK(human != 0);
        // No m.update() — this is the exact state at join time.
        const std::string out = survey(m, human);
        // The human spawns at their base: far from the point, so the
        // point line carries a distance and a compass bearing.
        CHECK(out.find("the point:") != std::string::npos);
        CHECK(out.find("YOU ARE ON IT") == std::string::npos);
        CHECK(out.find("east") != std::string::npos ||
              out.find("west") != std::string::npos);
        // Both teams exist, so a nearest enemy is named with a bearing.
        CHECK(out.find("nearest enemy:") != std::string::npos);
        std::cout << "[1] " << out;
    }
    // ---------- 2: DOTGOO join-time survey ----------
    {
        EventBus bus; GameClock clock; RNG rng(7);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        m.addBot(0);
        m.botfill();
        const uint64_t human = m.joinAsHuman(1);
        CHECK(human != 0);
        const std::string out = survey(m, human);
        // No point in DOTGOO — but the enemy readout still fires.
        CHECK(out.find("the point:") == std::string::npos);
        CHECK(out.find("nearest enemy:") != std::string::npos);
        std::cout << "[2] " << out;
    }
    // ---------- 3: join with no enemies ----------
    {
        EventBus bus; GameClock clock; RNG rng(7);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        m.addBot(0); // single slot, no other players
        const uint64_t human = m.joinAsHuman(0);
        CHECK(human != 0);
        const std::string out = survey(m, human);
        CHECK(out.find("the point:") != std::string::npos);
        CHECK(out.find("no living enemies") != std::string::npos);
        std::cout << "[3] " << out;
    }
    if (failures == 0) std::cout << "joinsurvey: ALL OK\n";
    return failures == 0 ? 0 : 1;
}
