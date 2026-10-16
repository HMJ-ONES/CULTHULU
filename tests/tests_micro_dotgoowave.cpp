// CULT-ULHU micro-dev regression tests: `match status` in DOTGOO shows
// the next minion-wave countdown.
//
// Minion waves spawn every MobaDefense::WAVE_INTERVAL seconds (skipped per
// team above the minion cap), but the status header only showed the live
// minion count — a player timing a lane push couldn't see the next wave
// coming. The header now prints "wave in Ns" from the same timer that
// spawns the waves. These pin:
//
// 1. The DOTGOO header contains "wave in <N>s".
// 2. The countdown decrements with match time (25s after 5s elapse).
// 3. It never reads 0s while a wave is still inbound (ceiling display).
// 4. Domination has no minion waves, so its header shows no wave line
//    (control against the string landing in the wrong mode).

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

static std::string dotgooStatus(double advanceSec, int seed) {
    EventBus bus; GameClock clock; RNG rng(seed);
    Match m(bus, clock, rng);
    if (!m.start("moba")) return "";
    m.botfill();
    for (int i = 0; i < static_cast<int>(advanceSec) && !m.isOver(); ++i)
        m.update(1.0);
    std::ostringstream oss;
    m.printStatus(oss);
    return oss.str();
}

int main() {
    const int interval = static_cast<int>(MobaDefense::WAVE_INTERVAL);
    CHECK(interval == 30);

    // ---------- header shows a live countdown at match start ----------
    {
        std::string s = dotgooStatus(0.0, 11);
        CHECK(s.find("match DOTGOO") != std::string::npos);
        // At t=0 the timer is fresh: full interval, ceiling display.
        CHECK(s.find("wave in " + std::to_string(interval) + "s") !=
              std::string::npos);
    }
    // ---------- countdown decrements with match time ----------
    {
        std::string s = dotgooStatus(5.0, 12);
        CHECK(s.find("wave in " + std::to_string(interval - 5) + "s") !=
              std::string::npos);
    }
    // ---------- one second before the wave: reads 1s, never 0s ----------
    {
        std::string s = dotgooStatus(29.0, 13);
        CHECK(s.find("wave in 1s") != std::string::npos);
        CHECK(s.find("wave in 0s") == std::string::npos);
    }
    // ---------- after a wave spawns the countdown wraps, never negative --
    {
        std::string s = dotgooStatus(35.0, 14);
        CHECK(s.find("wave in " + std::to_string(interval - 5) + "s") !=
              std::string::npos);
        CHECK(s.find("wave in -") == std::string::npos);
    }
    // ---------- Domination header has no wave line (control) ----------
    {
        EventBus bus; GameClock clock; RNG rng(15);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        m.botfill();
        std::ostringstream oss;
        m.printStatus(oss);
        CHECK(oss.str().find("wave in ") == std::string::npos);
    }

    if (failures == 0) std::cout << "ALL DOTGOO WAVE-TIMER CHECKS PASSED\n";
    return failures == 0 ? 0 : 1;
}
