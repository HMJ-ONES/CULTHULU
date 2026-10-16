// CULT-ULHU micro-dev regression tests: `match status` in DOTGOO shows the
// match time limit.
//
// Domination's status header shows "(limit 600s)" but the DOTGOO header
// never did — a player typing `match status` in a 20-minute MOBA couldn't
// see how close the whistle was. The DOTGOO branch now prints the limit
// from the same constant that ends the match. These pin:
//
// 1. The DOTGOO status header contains "(limit 1200s)".
// 2. The printed number is exactly MobaDefense::TIME_LIMIT (no drift).

#include "modes/Match.h"
#include "modes/MobaDefense.h"
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

int main() {
    // ---------- DOTGOO status header shows the time limit ----------
    {
        EventBus bus; GameClock clock; RNG rng(7);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        m.botfill();
        std::ostringstream oss;
        m.printStatus(oss);
        const std::string s = oss.str();
        const std::string expect =
            "(limit " + std::to_string(static_cast<int>(MobaDefense::TIME_LIMIT)) + "s)";
        CHECK(s.find(expect) != std::string::npos);
        // Sanity: the DOTGOO header actually rendered (guard against the
        // test matching a different line if the header is ever renamed).
        CHECK(s.find("match DOTGOO") != std::string::npos);
    }
    // ---------- Domination status still shows its limit (control) ----------
    {
        EventBus bus; GameClock clock; RNG rng(8);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        m.botfill();
        std::ostringstream oss;
        m.printStatus(oss);
        const std::string s = oss.str();
        CHECK(s.find("match domination") != std::string::npos);
        CHECK(s.find("(limit 600s)") != std::string::npos);
    }

    if (failures == 0) std::cout << "ALL TESTS PASSED\n";
    return failures == 0 ? 0 : 1;
}
