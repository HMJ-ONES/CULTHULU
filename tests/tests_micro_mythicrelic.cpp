// CULT-ULHU micro-dev regression tests: `match status` in DOTGOO shows
// the tier-3 mythic relic's countdown / live / claimed state.
//
// The mythic relic manifests at match t=600s with a global announcement,
// but the status header never said when it lands — a player planning for
// the endgame had no way to see the arrival. The header now prints one
// "mythic relic" line from the same spawnTime that fires RelicManifested
// (ceiling display, so it never reads 0s while still inbound). These pin:
//
// 1. At t=0 the header shows the full 600s countdown with the relic's name.
// 2. The countdown decrements with match time.
// 3. One second before manifest it reads 1s, never 0s.
// 4. After t=600 the relic reads LIVE at center with the 6s channel hint.
// 5. Domination shows no mythic-relic line (control against the string
//    landing in the wrong mode).

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
    // ---------- header shows the full countdown at match start ----------
    {
        std::string s = dotgooStatus(0.0, 21);
        CHECK(s.find("match DOTGOO") != std::string::npos);
        CHECK(s.find("mythic relic \"") != std::string::npos);
        CHECK(s.find("manifests in 600s") != std::string::npos);
    }
    // ---------- countdown decrements with match time ----------
    {
        std::string s = dotgooStatus(5.0, 22);
        CHECK(s.find("manifests in 595s") != std::string::npos);
    }
    // ---------- one second before manifest: 1s, never 0s ----------
    {
        std::string s = dotgooStatus(599.0, 23);
        CHECK(s.find("manifests in 1s") != std::string::npos);
        CHECK(s.find("manifests in 0s") == std::string::npos);
    }
    // ---------- after the 10:00 mark the relic is live ----------
    {
        std::string s = dotgooStatus(601.0, 21);
        CHECK(s.find("mythic relic \"") != std::string::npos);
        CHECK(s.find("LIVE at center (6s channel to claim)") !=
              std::string::npos);
        CHECK(s.find("manifests in ") == std::string::npos);
    }
    // ---------- Domination has no relic line (control) ----------
    {
        EventBus bus; GameClock clock; RNG rng(24);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        m.botfill();
        std::ostringstream oss;
        m.printStatus(oss);
        CHECK(oss.str().find("mythic relic") == std::string::npos);
    }

    if (failures == 0) std::cout << "OK\n";
    return failures == 0 ? 0 : 1;
}
