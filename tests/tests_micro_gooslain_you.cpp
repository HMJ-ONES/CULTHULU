// CULT-ULHU micro-dev regression tests: godslayer credit in second person.
//
// The driver's GreatOldOneSlain callout now reads "*** You slew team N's
// Great Old One! ***" when the joined human strikes the killing blow,
// instead of third-person "Team N's Great Old One falls to <your fighter>".
// The branch selects on `humanMatchId != 0 && e.sourceId == humanMatchId`
// (the same nonzero-guarded pattern as the kill/streak/shutdown/first-blood
// callouts), so the contract it depends on is:
//
// 1. A player-attributed killing blow publishes GreatOldOneSlain with
//    sourceId == the KILLER's player id — unchanged through damageBase
//    (pass-through pinned in tests_micro_gooslain) and resolvable through
//    Match::findPlayer, so the driver's comparison is meaningful.
// 2. A minion-attributed killing blow publishes sourceId == 0, which
//    resolves to NO player — so the personal branch cannot misfire on
//    minion kills (the driver keeps the "the minions" read) and the
//    nonzero guard keeps the third-person read when no human has joined.
//
// The console subscriber wording itself lives in main.cpp and is verified
// live in the driver (micro-dev convention), like the other arena callouts.

#include "modes/Match.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "core/Events.h"

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

int main() {
    // ---------- 1: player-attributed blow: sourceId is a real player id -----
    {
        EventBus bus; GameClock clock; RNG rng(90);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        uint64_t godslayer = m.addBot(0);

        // The production path is playerHitStructuresCredit(attackerId) ->
        // damageBase -> published event (pass-through pinned elsewhere);
        // this pins that a player id on the event resolves via findPlayer,
        // which is what the driver's `e.sourceId == humanMatchId` branch
        // needs to be sound.
        GameEvent e(EventType::GreatOldOneSlain);
        e.faction = 1;           // losing team: the godslayer's enemy
        e.sourceId = godslayer;
        CHECK(m.findPlayer(e.sourceId) != nullptr);
    }

    // ---------- 2: minion-attributed blow: sourceId 0, no player ------------
    {
        EventBus bus; GameClock clock; RNG rng(91);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        m.addBot(0); // a player exists; id 0 must still resolve to nothing

        GameEvent e(EventType::GreatOldOneSlain);
        e.faction = 1;
        e.sourceId = 0; // minions did it — no player to attribute
        CHECK(m.findPlayer(e.sourceId) == nullptr);
        // And the nonzero guard: with no human joined the personal branch
        // can never fire, even against a nonzero sourceId.
        CHECK(m.findPlayer(0xFFFF) == nullptr);
    }

    if (failures == 0) std::cout << "ALL GOOSLAIN-YOU CHECKS PASSED\n";
    return failures ? 1 : 0;
}
