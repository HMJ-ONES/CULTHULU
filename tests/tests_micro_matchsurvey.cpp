// CULT-ULHU micro-dev regression tests: match-mode arrival survey.
//
// The driver's `move` in a live match used to print only raw coordinates —
// a Domination player positioning around the central point had to type
// `match status` to learn whether they were even on it, and in either
// mode they had no idea whether an enemy was closing in. The driver now
// calls Match::printMatchSurvey after every match move. These pin the
// contract:
//
// 1. Domination, human moved onto the point alone: "YOU ARE ON IT" +
//    their team banking (after one tick recomputes the holder).
// 2. Domination, human far from the point: "the point:" line with a
//    distance + compass bearing, and no "ON IT".
// 3. Domination, enemy also on the point: "contested".
// 4. Nearest living enemy is named with distance + bearing ("36m
//    east-north" for a (30,0,-20) enemy vs a (0,0,0) human).
// 5. No enemies at all: "no living enemies".
// 6. Unknown human id: empty output (never crashes, never prints).

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
    // ---------- 1: on the point alone -> ON IT + banking ----------
    {
        EventBus bus; GameClock clock; RNG rng(7);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        m.addBot(0);
        const uint64_t human = m.joinAsHuman(0);
        CHECK(human != 0);
        m.botfill();
        m.movePlayer(human, Vec3(0, 0, 0));
        m.update(1.0); // recompute holder/occupants
        const std::string out = survey(m, human);
        CHECK(out.find("YOU ARE ON IT") != std::string::npos);
        CHECK(out.find("banking") != std::string::npos);
        CHECK(out.find("team 0") != std::string::npos);
        std::cout << "[1] " << out;
    }
    // ---------- 2: far from the point ----------
    {
        EventBus bus; GameClock clock; RNG rng(7);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        m.addBot(0);
        const uint64_t human = m.joinAsHuman(0);
        CHECK(human != 0);
        m.movePlayer(human, Vec3(-80, 0, -80));
        m.update(1.0);
        const std::string out = survey(m, human);
        CHECK(out.find("the point:") != std::string::npos);
        CHECK(out.find("YOU ARE ON IT") == std::string::npos);
        // Point sits at (0,0,0): from (-80,-80) that is east + south.
        CHECK(out.find("east-south") != std::string::npos);
        std::cout << "[2] " << out;
    }
    // ---------- 3: enemy on the point too -> contested ----------
    {
        EventBus bus; GameClock clock; RNG rng(7);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        m.addBot(0);
        const uint64_t human = m.joinAsHuman(0);
        CHECK(human != 0);
        m.botfill();
        // Put an enemy on the point with the human.
        for (size_t i = 0; i < m.playerCount(); ++i) {
            const auto& p = m.player(i);
            if (p.id != human && p.team == 1 && p.alive) {
                m.movePlayer(p.id, Vec3(2, 0, 1));
                break;
            }
        }
        m.movePlayer(human, Vec3(0, 0, 0));
        m.update(1.0);
        const std::string out = survey(m, human);
        CHECK(out.find("YOU ARE ON IT") != std::string::npos);
        CHECK(out.find("contested") != std::string::npos);
        std::cout << "[3] " << out;
    }
    // ---------- 4: nearest enemy named with distance + bearing ----------
    {
        EventBus bus; GameClock clock; RNG rng(7);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        m.addBot(0);
        const uint64_t human = m.joinAsHuman(0);
        CHECK(human != 0);
        const uint64_t foe = m.addBot(1);
        CHECK(foe != 0);
        m.movePlayer(human, Vec3(0, 0, 0));
        m.movePlayer(foe, Vec3(30, 0, -20));
        const std::string out = survey(m, human);
        CHECK(out.find("nearest enemy:") != std::string::npos);
        CHECK(out.find("Bot") != std::string::npos);
        CHECK(out.find("36m") != std::string::npos); // sqrt(30^2+20^2)=36.05
        CHECK(out.find("east-north") != std::string::npos);
        CHECK(out.find("no living enemies") == std::string::npos);
        std::cout << "[4] " << out;
    }
    // ---------- 5: no enemies -> no living enemies ----------
    {
        EventBus bus; GameClock clock; RNG rng(7);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        m.addBot(0);
        const uint64_t human = m.joinAsHuman(0);
        CHECK(human != 0);
        m.movePlayer(human, Vec3(10, 0, 10));
        const std::string out = survey(m, human);
        CHECK(out.find("no living enemies") != std::string::npos);
        std::cout << "[5] " << out;
    }
    // ---------- 6: unknown human id -> empty ----------
    {
        EventBus bus; GameClock clock; RNG rng(7);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        CHECK(survey(m, 424242).empty());
    }

    if (failures == 0) std::cout << "ALL MATCHSURVEY TESTS PASSED\n";
    return failures == 0 ? 0 : 1;
}
