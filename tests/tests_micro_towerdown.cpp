// CULT-ULHU micro-dev regression tests: DOTGOO "tower down" announcement.
//
// DOTGOO towers fell silently: a crumbled lane defense produced no console
// callout and no line in the exported match log — the match's story jumped
// from pushing lanes to the GOO fight with nothing in between. Towers now
// publish EventType::TowerDestroyed exactly once (MobaDefense guards the
// hp crossing, so repeated hits on a dead tower stay quiet), which gets a
// console callout (main.cpp), a TOWER line in the GitHub match log
// (MatchLogger), and the raze FX preset. These pin:
//
// 1. Player blow: playerHitStructuresCredit crossing zero publishes exactly
//    one TowerDestroyed with the fallen tower's team, lane index, lane name
//    ("mid" for lane 1), and the attacker's id; the return value still
//    reports destruction (the wave-34 XP contract).
// 2. No double-publish: hitting the same dead tower again fires nothing.
// 3. Minion path: a boosted minion grinding a tower down in the update loop
//    publishes once with the tower's team/lane.
// 4. MatchLogger: a TowerDestroyed on the bus lands a TOWER line carrying
//    the team, lane name, and the killer attribution.

#include "modes/MobaDefense.h"
#include "modes/Match.h"
#include "modes/MatchLogger.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"

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
    // ---------- 1+2: player blow path ----------
    {
        EventBus bus; GameClock clock; RNG rng(21);
        MobaDefense moba(bus, clock, rng);
        moba.setupDefaultMap();
        moba.update(0.1); // towers auto-build: 3 lanes x 2 teams x 2

        int hits = 0;
        GameEvent last;
        bus.subscribe(EventType::TowerDestroyed,
                      [&](const GameEvent& e) { ++hits; last = e; });

        // Team-1 mid tower (lane 1), point-blank blow from player 4242.
        Vec3 tp = moba.towerPos(1, 1, 0);
        const bool destroyed =
            moba.playerHitStructuresCredit(0, tp, 20.0f, 99999.0f, 4242);
        CHECK(destroyed);
        CHECK(hits == 1);
        CHECK(last.sourceId == 4242);
        CHECK(last.faction == 1);       // the fallen tower's team
        CHECK(last.amount == 1.0);      // lane index
        CHECK(last.tag == "mid");       // lane name for lane 1

        // 2. The same tower hit again: nothing to destroy, no new event.
        // (Range 5m so the adjacent lane's tower 8m away is untouched.)
        CHECK(!moba.playerHitStructuresCredit(0, tp, 5.0f, 99999.0f, 4242));
        CHECK(hits == 1);
    }

    // ---------- 3: minion grind path ----------
    {
        EventBus bus; GameClock clock; RNG rng(22);
        MobaDefense moba(bus, clock, rng);
        moba.setupDefaultMap();
        moba.update(0.1);

        int hits = 0;
        GameEvent last;
        bus.subscribe(EventType::TowerDestroyed,
                      [&](const GameEvent& e) { ++hits; last = e; });

        // Team-0 minion parked on team-1's top-lane tower, boosted.
        moba.spawnWave(0, 0);
        CHECK(!moba.minions().empty());
        MobaDefense::Minion& m = moba.minions().front();
        m.pos = moba.towerPos(1, 0, 0);
        m.hp = m.maxHp = 1e9f; // towers shoot back; stay standing
        m.dps = 1e6f;
        m.range = 5.0f; // reach only the tower at this spot, not its lane-mate
        moba.update(1.0);
        CHECK(hits == 1);
        CHECK(last.faction == 1);
        CHECK(last.amount == 0.0);
        CHECK(last.tag == "top");
        // Killer id: the minion's own id (minion kills are not player kills).
        CHECK(last.sourceId == m.id);
        // The tower is gone from the alive set.
        CHECK(moba.towerHp(1, 0, 0) <= 0.0f);
        // One more tick: still exactly one event (crossing guard holds).
        moba.update(1.0);
        CHECK(hits == 1);
    }

    // ---------- 4: MatchLogger line ----------
    {
        EventBus bus; GameClock clock; RNG rng(23);
        Match m(bus, clock, rng);
        CHECK(m.start("moba"));
        MatchLogger logger;
        logger.attach(m);

        GameEvent e(EventType::TowerDestroyed);
        e.faction = 1;
        e.amount = 1.0;
        e.tag = "mid";
        e.sourceId = 0; // minions did it — no player to attribute
        bus.publish(e);

        int towers = 0;
        for (const auto& entry : logger.events()) {
            if (entry.kind == "TOWER") {
                ++towers;
                CHECK(entry.text.find("t1") != std::string::npos);
                CHECK(entry.text.find("mid") != std::string::npos);
                CHECK(entry.text.find("minions") != std::string::npos);
            }
        }
        CHECK(towers == 1);
    }

    if (failures == 0) std::cout << "ALL TOWERDOWN CHECKS PASSED\n";
    return failures ? 1 : 0;
}
