// CULT-ULHU wave 43 regression tests: AFK takeover state machine.
// The commit "AFK takeover, select countdown, rejoin" (Oct 12) shipped with
// zero coverage, and recent experience says fresh untested paths are where
// the bugs live. These pin the reward-relevant behavior:
//
// 1. A fresh player is never AFK; afkFraction is 0 with no match time
//    elapsed (the division-by-zero guard fires, not NaN).
// 2. Going AFK at match start and staying away: fraction == 1.0.
// 3. Going AFK mid-match: fraction is exactly the AFK share.
// 4. Coming back: the frozen stretch stops accumulating; later time
//    dilutes the fraction.
// 5. setPlayerAfk(true) twice in a row does NOT reset afkSince — the
//    whole stretch from the first flag counts (this is the one that
//    would silently cut a player's earned-rewards share).
// 6. Unknown player ids are safe: isPlayerAfk false, setPlayerAfk no-op,
//    afkFraction 0.0.

#include "modes/Match.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

#include <cmath>
#include <iostream>

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

static void afk_checks() {
    // ---------- fresh player: never AFK, zero-time fraction is safe ----------
    {
        EventBus bus; GameClock clock; RNG rng(430);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t h = m.addPlayer("Human", 0);
        CHECK(!m.isPlayerAfk(h));
        m.setPlayerAfk(h, true); // before any tick
        CHECK(m.afkFraction(h) == 0.0);
        CHECK(!std::isnan(m.afkFraction(h)));
    }
    // ---------- away the whole match: fraction 1.0 ----------
    {
        EventBus bus; GameClock clock; RNG rng(431);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t h = m.addPlayer("Human", 0);
        m.setPlayerAfk(h, true);
        for (int i = 0; i < 60; ++i) m.update(1.0);
        CHECK(m.isPlayerAfk(h));
        CHECK(std::fabs(m.afkFraction(h) - 1.0) < 1e-9);
    }
    // ---------- AFK mid-match: exactly the AFK share ----------
    {
        EventBus bus; GameClock clock; RNG rng(432);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t h = m.addPlayer("Human", 0);
        for (int i = 0; i < 30; ++i) m.update(1.0); // present 0-30s
        m.setPlayerAfk(h, true);                     // away from t=30
        for (int i = 0; i < 30; ++i) m.update(1.0);
        CHECK(std::fabs(m.afkFraction(h) - 0.5) < 1e-9);
    }
    // ---------- coming back freezes the stretch; later time dilutes ----------
    {
        EventBus bus; GameClock clock; RNG rng(433);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t h = m.addPlayer("Human", 0);
        m.setPlayerAfk(h, true); // away 0-30s
        for (int i = 0; i < 30; ++i) m.update(1.0);
        m.setPlayerAfk(h, false); // back at t=30
        CHECK(!m.isPlayerAfk(h));
        for (int i = 0; i < 60; ++i) m.update(1.0); // back 30-90s
        // 30s away of 90s total = 1/3.
        CHECK(std::fabs(m.afkFraction(h) - 1.0 / 3.0) < 1e-9);
    }
    // ---------- double flag does not restart the AFK clock ----------
    {
        EventBus bus; GameClock clock; RNG rng(434);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        uint64_t h = m.addPlayer("Human", 0);
        for (int i = 0; i < 10; ++i) m.update(1.0); // present 0-10s
        m.setPlayerAfk(h, true);                     // away from t=10
        for (int i = 0; i < 10; ++i) m.update(1.0);
        m.setPlayerAfk(h, true); // stray second flag — must be a no-op
        for (int i = 0; i < 10; ++i) m.update(1.0);
        // Away from t=10 to t=30: 20s of 30s = 2/3. A reset afkSince
        // would wrongly report 10s/30s = 1/3.
        CHECK(std::fabs(m.afkFraction(h) - 2.0 / 3.0) < 1e-9);
    }
    // ---------- unknown ids: safe no-ops ----------
    {
        EventBus bus; GameClock clock; RNG rng(435);
        Match m(bus, clock, rng);
        CHECK(m.start("capture"));
        CHECK(!m.isPlayerAfk(99999));
        m.setPlayerAfk(99999, true); // must not crash
        CHECK(m.afkFraction(99999) == 0.0);
    }
}

int main() {
    afk_checks();
    if (failures == 0) std::cout << "wave43: all checks passed\n";
    else std::cout << "wave43: " << failures << " checks FAILED\n";
    return failures == 0 ? 0 : 1;
}
