// CULT-ULHU micro-dev regression tests: tower-toppling credit in second
// person.
//
// The driver's TowerDestroyed callout now reads "*** You toppled team N's
// <lane> tower! ***" when the joined human strikes the killing blow,
// instead of third-person "team N's <lane> tower falls to <your fighter>".
// The branch selects on `humanMatchId != 0 && e.sourceId == humanMatchId`
// (the same nonzero-guarded pattern as the kill/streak/shutdown/first-blood/
// godslayer callouts), so the contract it depends on is:
//
// 1. A player-attributed tower kill publishes TowerDestroyed with
//    sourceId == the ATTACKER's player id — unchanged through
//    playerHitStructuresCredit (pinned in tests_micro_towerdown) and
//    resolvable through Match::findPlayer, so the driver's comparison
//    against humanMatchId is meaningful and cannot land on a minion.
// 2. A minion-attributed kill carries the minion's own id (never 0) — and
//    minion ids live in a separate namespace from player ids
//    (players: kPlayerIdBase 1000+; minions: from 1), so the driver's
//    "the minions" fallback is sound: findPlayer cannot resolve them and
//    the personal branch cannot misfire on minion kills.
//
// The console subscriber wording itself lives in main.cpp and is verified
// live in the driver (micro-dev convention), like the other arena callouts.

#include "modes/MobaDefense.h"
#include "modes/Match.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

#include <iostream>
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
    // ---------- 1: player blow: sourceId resolves to a match player --------
    {
        EventBus bus; GameClock clock; RNG rng(31);
        MobaDefense moba(bus, clock, rng);
        moba.setupDefaultMap();
        moba.update(0.1); // towers auto-build: 3 lanes x 2 teams x 2

        GameEvent last(EventType::TowerDestroyed);
        bool fired = false;
        bus.subscribe(EventType::TowerDestroyed,
                      [&](const GameEvent& e) { last = e; fired = true; });

        // A real match player id — the value the driver's
        // `e.sourceId == humanMatchId` comparison sees.
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        const uint64_t playerId = m.addBot(0);

        // Team-1 mid tower (lane 1), point-blank blow from the player.
        Vec3 tp = moba.towerPos(1, 1, 0);
        CHECK(moba.playerHitStructuresCredit(0, tp, 20.0f, 99999.0f,
                                             playerId));
        CHECK(fired);
        CHECK(last.sourceId == playerId);
        // The driver's personal branch is sound: the id resolves to the
        // player, so the "You toppled..." read cannot misattribute.
        CHECK(m.findPlayer(last.sourceId) != nullptr);
        CHECK(last.tag == "mid");
    }

    // ---------- 2: minion grind: never resolves to a player ---------------
    {
        EventBus bus; GameClock clock; RNG rng(32);
        MobaDefense moba(bus, clock, rng);
        moba.setupDefaultMap();
        moba.update(0.1);

        GameEvent last(EventType::TowerDestroyed);
        bool fired = false;
        bus.subscribe(EventType::TowerDestroyed,
                      [&](const GameEvent& e) { last = e; fired = true; });

        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        const uint64_t humanId = m.addBot(0);

        // Team-0 minion parked on team-1's top-lane tower, boosted.
        moba.spawnWave(0, 0);
        CHECK(!moba.minions().empty());
        MobaDefense::Minion& mi = moba.minions().front();
        mi.pos = moba.towerPos(1, 0, 0);
        mi.hp = mi.maxHp = 1e9f; // towers shoot back; stay standing
        mi.dps = 1e6f;
        mi.range = 5.0f; // reach only the tower at this spot, not its lane-mate
        moba.update(1.0);
        CHECK(fired);
        // Killer id is the minion's own id — not a player id, so the
        // driver's findPlayer falls through to "the minions".
        CHECK(m.findPlayer(last.sourceId) == nullptr);
        CHECK(last.sourceId != humanId);
        CHECK(last.sourceId != 0);
    }

    if (failures == 0) std::cout << "tests_micro_towerdown_you: all green\n";
    return failures ? 1 : 0;
}
