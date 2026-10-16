// CULT-ULHU micro-dev regression tests: dead-player respawn countdown in
// `match status` roster rows.
//
// Dead players used to print a bare "DEAD" in the roster row with no hint of
// when they come back — the respawn timer lived only in the `mcast` failure
// message, so a dead player reading `match status` saw no countdown. The row
// now reads "DEAD (respawn Ns)". These pin (staging a real duel so the death
// goes through the production killPlayer path, which arms the 5s Domination
// respawn timer):
//
// 1. A fresh match has no DEAD rows at all (no behavior change when alive).
// 2. The victim's row shows "DEAD (respawn 4s)" right after the killing tick
//    (the tick already ate 1s of the 5s timer).
// 3. The countdown decays with game time ("respawn 1s" after +3s), and the
//    DEAD row is gone once the player revives.

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

static std::string statusOf(Match& m) {
    std::ostringstream oss;
    m.printStatus(oss);
    return oss.str();
}

int main() {
    EventBus bus; GameClock clock; RNG rng(8);
    Match m(bus, clock, rng);
    CHECK(m.start("capture"));
    uint64_t a = m.addBot(0);
    uint64_t b = m.addBot(1);
    // Identical kits — the duel is decided by update order and directional
    // facing, not kit asymmetry (same staging as tests_wave21).
    CHECK(m.setPlayerKit(a, "cthulhu_avatar"));
    CHECK(m.setPlayerKit(b, "cthulhu_avatar"));
    m.movePlayer(a, Vec3(0, 0, 0));
    m.movePlayer(b, Vec3(1, 0, 0));
    CHECK(m.setFacingYaw(a, 0.0f));

    // 1. Fresh match: nobody dead, no DEAD rows.
    CHECK(statusOf(m).find("DEAD") == std::string::npos);

    // 2. Fight until a kills b (proven deterministic in tests_wave21).
    int ticks = 0;
    while (m.findPlayer(b)->alive && ticks < 30) {
        m.update(1.0);
        ++ticks;
    }
    CHECK(!m.findPlayer(b)->alive);
    CHECK(ticks < 30);
    CHECK(statusOf(m).find("DEAD (respawn 4s)") != std::string::npos);

    // 3. Countdown decays; row clears on revive.
    m.update(3.0);
    CHECK(!m.findPlayer(b)->alive);
    CHECK(statusOf(m).find("DEAD (respawn 1s)") != std::string::npos);
    m.update(1.0);
    CHECK(m.findPlayer(b)->alive);
    CHECK(statusOf(m).find("DEAD") == std::string::npos);

    if (failures == 0) std::cout << "ALL RESPAWN-COUNTDOWN CHECKS PASSED\n";
    return failures ? 1 : 0;
}
